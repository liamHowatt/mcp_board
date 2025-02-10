#include "mcp_modnet_server.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	MMN_SRV_BLOCKED_ON_FAULT = 0,
	MMN_SRV_BLOCKED_ON_TRANSFER,
	MMN_SRV_BLOCKED_ON_POLL,
	MMN_SRV_BLOCKED_ON_XPOINT
} mmn_srv_blocked_on_t;

typedef enum {
	MMN_SRV_OP_LOCK_NONE = 0,
	MMN_SRV_OP_LOCK_WRITE,
	MMN_SRV_OP_LOCK_READ,
	MMN_SRV_OP_LOCK_POLL
} mmn_srv_op_lock_t;

static uint32_t bits_needed_to_represent_value(uint32_t value)
{
	uint32_t y = 1;
	while(value >>= 1) y++;
	return y;
}

static mmn_srv_t * srv_from_soc(mmn_srv_socket_t * soc)
{
	uint8_t * soc_raw = (uint8_t *) soc;
	soc_raw -= offsetof(mmn_srv_t, memb[soc->index].soc);
	return (mmn_srv_t *) soc_raw;
}

static void buf_push(mmn_srv_buf_t * buf, uint8_t buf_size, uint8_t to_push)
{
	size_t next_in = buf->next_out + buf->len;
	if(next_in >= buf_size) next_in -= buf_size;
	buf->data[next_in] = to_push;
	buf->len += 1;
}

static uint8_t buf_pop(mmn_srv_buf_t * buf, uint8_t buf_size)
{
	uint8_t ret = buf->data[buf->next_out];
	buf->next_out += 1;
	if(buf->next_out == buf_size) buf->next_out = 0;
	buf->len -= 1;
	return ret;
}

static mmn_srv_buf_t * get_buf(mmn_srv_t * srv, uint8_t sender, uint8_t receiver)
{
	size_t one_buf_size = sizeof(mmn_srv_buf_t) + srv->buf_size;
	return (mmn_srv_buf_t *) &srv->bufs[(srv->socket_count * receiver + sender) * one_buf_size];
}

static mmn_srv_flags_t * get_flags(mmn_srv_t * srv, uint8_t owner_session, uint8_t interest_session)
{
	return &srv->flags[srv->socket_count * owner_session + interest_session];
}

static void block_on(mmn_srv_socket_t * soc, mmn_srv_blocked_on_t block_on, mmn_srv_state_machine_cb_t then_cb)
{
	soc->blocked_on = block_on;
	soc->state_machine_cb = then_cb;
}

static void transfer(mmn_srv_socket_t * soc, mbb_srv_transfer_t trn, mmn_srv_state_machine_cb_t then_cb)
{
	srv_from_soc(soc)->cbs->set_trn(soc->ctx, trn);
	block_on(soc, MMN_SRV_BLOCKED_ON_TRANSFER, then_cb);
}

static void read_1_state(mmn_srv_socket_t * soc);
static void read_1_state_cb(mmn_srv_socket_t * soc)
{
	read_1_state(soc);
}
static void read_1_state(mmn_srv_socket_t * soc)
{
	if(soc->state_read_i > 0) {
		*(soc->state_read_dst++) = soc->read_byte;
	}
	if(soc->state_read_i < soc->state_read_len) {
		transfer(soc, MBB_SRV_BYTE_TRANSFER_READ, read_1_state_cb);
		soc->state_read_i += 1;
	} else {
		soc->state_read_then_cb(soc);
	}
}
static void read_state(mmn_srv_socket_t * soc, size_t len, uint8_t * dst, mmn_srv_state_machine_cb_t then_cb)
{
	soc->state_read_then_cb = then_cb;
	soc->state_read_dst = dst;
	soc->state_read_i = 0;
	soc->state_read_len = len;
	read_1_state(soc);
}

static void write_1_state(mmn_srv_socket_t * soc);
static void write_1_state_cb(mmn_srv_socket_t * soc)
{
	write_1_state(soc);
}
static void write_1_state(mmn_srv_socket_t * soc)
{
	if(soc->state_write_i < soc->state_write_len) {
		transfer(soc, soc->state_write_src[soc->state_write_i], write_1_state_cb);
		soc->state_write_i += 1;
	} else {
		soc->state_write_then_cb(soc);
	}
}
static void write_state(mmn_srv_socket_t * soc, size_t len, const uint8_t * src, mmn_srv_state_machine_cb_t then_cb)
{
	soc->state_write_then_cb = then_cb;
	soc->state_write_src = src;
	soc->state_write_i = 0;
	soc->state_write_len = len;
	write_1_state(soc);
}

static void poll_state(mmn_srv_socket_t * soc, mmn_srv_state_machine_cb_t then_cb)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	mmn_srv_session_t * sess = &srv->memb[soc->token].sess;
	sess->poll_blocked = true;
	block_on(soc, MMN_SRV_BLOCKED_ON_POLL, then_cb);
}

static void crosspoint_state(mmn_srv_socket_t * soc, uint32_t transfer_data, mmn_srv_state_machine_cb_t then_cb)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	srv->cbs->set_xpoint(srv, transfer_data);
	block_on(soc, MMN_SRV_BLOCKED_ON_XPOINT, then_cb);
}

static bool time_is_up(mmn_srv_socket_t * soc)
{
	uint32_t timeout = soc->req_state_poll_timeout;
	if(timeout == 0) return true;
	if(timeout == 255) return false;
	timeout *= timeout;
	mmn_srv_t * srv = srv_from_soc(soc);
	uint32_t time_now = srv->cbs->get_tick_ms(srv);
	uint32_t time_elapsed = time_now - soc->req_state_poll_start_time;
	return time_elapsed > timeout;
}

static void poll_notify_inner(mmn_srv_t * srv, uint8_t session)
{
	mmn_srv_session_t * sess = &srv->memb[session].sess;
	if(!sess->poll_blocked) return;
	if(srv->token_counter > sess->poll_reported_token_count) {
		sess->poll_blocked = false;
		return;
	}
	uint8_t total_sessions = srv->token_counter;
	for(uint8_t i = 0; i < total_sessions; i++) {
		mmn_srv_flags_t * flags = get_flags(srv, session, i);
		if(flags->interests & flags->ready) {
			sess->poll_blocked = false;
			return;
		}
	}
}
static void poll_notify(mmn_srv_t * srv, uint8_t session)
{
	if(session != 255) {
		poll_notify_inner(srv, session);
		return;
	}
	uint8_t total_sessions = srv->token_counter;
	for(uint8_t i = 0; i < total_sessions; i++) {
		poll_notify_inner(srv, i);
	}
}

static void req_3_state(mmn_srv_socket_t * soc);

static void req_15_state_cb(mmn_srv_socket_t * soc)
{
	req_3_state(soc);
}
static void req_15_state(mmn_srv_socket_t * soc)
{
	write_state(soc, 1, &soc->index, req_15_state_cb);
}

static void req_14_state_cb(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	srv->crosspoint_is_transferring = false;
	req_3_state(soc);
}
static void req_14_state(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	bool enable = soc->req_13_state_xpoint_data.pin_info & 0x01;
	soc->req_13_state_xpoint_data.pin_info >>= 1;
	uint32_t output_index = (soc->req_13_state_xpoint_data.output << 2)
							| (soc->req_13_state_xpoint_data.pin_info & 0x03);
	uint32_t input_idx = 0;
	if(soc->req_13_state_xpoint_data.input != 255) {
		soc->req_13_state_xpoint_data.pin_info >>= 2;
		input_idx = ((soc->req_13_state_xpoint_data.input << 2)
						| (soc->req_13_state_xpoint_data.pin_info & 0x03))
					+ 1;
	}
	uint32_t edge_point_count = 4 * srv->socket_count;
	uint32_t transfer_data = (edge_point_count + 1) * output_index + input_idx;
	if(enable) {
		transfer_data |= 1 << bits_needed_to_represent_value((edge_point_count + 1) * edge_point_count - 1);
	}
	crosspoint_state(soc, transfer_data, req_14_state_cb);
}

static void req_13_state_cb(mmn_srv_socket_t * soc)
{
	req_14_state(soc);
}
static void req_13_state(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	if(srv->crosspoint_is_transferring) return;
	srv->crosspoint_is_transferring = true;
	read_state(soc, sizeof(soc->req_13_state_xpoint_data), (uint8_t *) &soc->req_13_state_xpoint_data, req_13_state_cb);
}

static void req_12_state(mmn_srv_socket_t * soc);
static void req_12_state_cb(mmn_srv_socket_t * soc)
{
	req_3_state(soc);
}
static void req_12_1_state_cb(mmn_srv_socket_t * soc)
{
	req_12_state(soc);
}
static void req_12_state(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	mmn_srv_session_t * sess = &srv->memb[soc->token].sess;
	uint8_t flags_out = 0;
	size_t data_len = 1;

	if(srv->token_counter > sess->poll_reported_token_count) {
		sess->poll_reported_token_count = srv->token_counter;
		flags_out |= MMN_SRV_FLAG_PRESENCE;
		soc->req_12_state_event_data[data_len++] = srv->token_counter;
	}

	uint8_t starting_check = sess->poll_next_check;
	do {
		uint8_t check = sess->poll_next_check++;
		if(sess->poll_next_check == srv->token_counter) sess->poll_next_check = 0;

		mmn_srv_flags_t * flags = get_flags(srv, soc->token, check);
		uint8_t anded = flags->interests & flags->ready;
		if(anded) {
			flags->ready = 0;
			flags_out |= anded;
			soc->req_12_state_event_data[data_len++] = check;
			break;
		}

	} while(sess->poll_next_check != starting_check);

	if(flags_out || time_is_up(soc)) {
		soc->req_12_state_event_data[0] = flags_out;
		write_state(soc, data_len, soc->req_12_state_event_data, req_12_state_cb);
	}
	else {
		poll_state(soc, req_12_1_state_cb);
	}
}

static void req_11_state_cb(mmn_srv_socket_t * soc)
{
	req_12_state(soc);
}
static void req_11_state(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	uint8_t * op_lock = &srv->memb[soc->token].sess.op_lock;
	if(*op_lock == MMN_SRV_OP_LOCK_POLL) return;
	*op_lock = MMN_SRV_OP_LOCK_POLL;
	soc->req_state_poll_start_time = srv->cbs->get_tick_ms(srv);
	read_state(soc, 1, &soc->req_state_poll_timeout, req_11_state_cb);
}

static void req_10_state_cb(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	mmn_srv_flags_t * flags = get_flags(srv, soc->token, soc->req_10_state_intr_set.session);
	flags->interests = soc->req_10_state_intr_set.flags;
	flags->ready = 0;
	if((soc->req_10_state_intr_set.flags & MMN_SRV_FLAG_READABLE)) {
		mmn_srv_buf_t * buf = get_buf(srv, soc->req_10_state_intr_set.session, soc->token);
		if(buf->len) {
			flags->ready |= MMN_SRV_FLAG_READABLE;
		}
	}
	if(soc->req_10_state_intr_set.flags & MMN_SRV_FLAG_WRITABLE) {
		mmn_srv_buf_t * buf = get_buf(srv, soc->token, soc->req_10_state_intr_set.session);
		if(srv->buf_size - buf->len) {
			flags->ready |= MMN_SRV_FLAG_WRITABLE;
		}
	}
	poll_notify(srv, soc->token);
	req_3_state(soc);
}
static void req_10_state(mmn_srv_socket_t * soc)
{
	read_state(soc, sizeof(soc->req_10_state_intr_set), (uint8_t *) &soc->req_10_state_intr_set, req_10_state_cb);
}

static void req_9_1_state(mmn_srv_socket_t * soc);
static void req_9_1_state_cb(mmn_srv_socket_t * soc)
{
	req_9_1_state(soc);
}
static void req_9_1_state(mmn_srv_socket_t * soc)
{
	uint8_t full_size = soc->req_state_read_want_info.amount < soc->req_state_read_available_data
		? soc->req_state_read_want_info.amount : soc->req_state_read_available_data;

	if(soc->req_9_state_i < full_size) {
		soc->req_9_state_i += 1;
		mmn_srv_t * srv = srv_from_soc(soc);
		mmn_srv_buf_t * buf = get_buf(srv, soc->req_state_read_want_info.from, soc->token);
		if(buf->len == srv->buf_size) {
			mmn_srv_flags_t * flags = get_flags(srv, soc->req_state_read_want_info.from, soc->token);
			flags->ready |= MMN_SRV_FLAG_WRITABLE;
			poll_notify(srv, soc->req_state_read_want_info.from);
		}
		soc->req_9_state_reading_src = buf_pop(buf, srv->buf_size);
		write_state(soc, 1, &soc->req_9_state_reading_src, req_9_1_state_cb);
	}
	else {
		req_3_state(soc);
	}
}
static void req_9_state(mmn_srv_socket_t * soc)
{
	soc->req_9_state_i = 0;
	req_9_1_state(soc);
}

static void req_8_state_cb(mmn_srv_socket_t * soc)
{
	req_9_state(soc);
}
static void req_8_state(mmn_srv_socket_t * soc)
{
	mmn_srv_session_t * sess = &srv_from_soc(soc)->memb[soc->token].sess;
	if(
		sess->op_lock == MMN_SRV_OP_LOCK_READ
		&& sess->operating_on == soc->req_state_read_want_info.from
	) return;
	sess->op_lock = MMN_SRV_OP_LOCK_READ;
	sess->operating_on = soc->req_state_read_want_info.from;
	write_state(soc, 1, &soc->req_state_read_available_data, req_8_state_cb);
}

static void req_7_state_cb(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	mmn_srv_buf_t * buf = get_buf(srv, soc->req_state_read_want_info.from, soc->token);
	soc->req_state_read_available_data = buf->len;
	req_8_state(soc);
}
static void req_7_state(mmn_srv_socket_t * soc)
{
	read_state(soc, sizeof(soc->req_state_read_want_info), (uint8_t *) &soc->req_state_read_want_info, req_7_state_cb);
}

static void req_6_1_state(mmn_srv_socket_t * soc);
static void req_6_1_state_cb(mmn_srv_socket_t * soc)
{
	req_6_1_state(soc);
}
static void req_6_1_state(mmn_srv_socket_t * soc)
{
	uint8_t full_size = soc->req_state_write_want_info.amount < soc->req_state_write_free_space
		? soc->req_state_write_want_info.amount : soc->req_state_write_free_space;

	if(soc->req_6_state_i > 0) {
		mmn_srv_t * srv = srv_from_soc(soc);
		mmn_srv_buf_t * buf = get_buf(srv, soc->token, soc->req_state_write_want_info.recipient);
		if(buf->len == 0) {
			mmn_srv_flags_t * flags = get_flags(srv, soc->req_state_write_want_info.recipient, soc->token);
			flags->ready |= MMN_SRV_FLAG_READABLE;
			poll_notify(srv, soc->req_state_write_want_info.recipient);
		}
		buf_push(buf, srv->buf_size, soc->req_6_state_written_dst);
	}

	if(soc->req_6_state_i < full_size) {
		soc->req_6_state_i += 1;
		read_state(soc, 1, &soc->req_6_state_written_dst, req_6_1_state_cb);
	}
	else {
		req_3_state(soc);
	}
}
static void req_6_state(mmn_srv_socket_t * soc)
{
	soc->req_6_state_i = 0;
	req_6_1_state(soc);
}

static void req_5_state_cb(mmn_srv_socket_t * soc)
{
	req_6_state(soc);
}
static void req_5_state(mmn_srv_socket_t * soc)
{
	mmn_srv_session_t * sess = &srv_from_soc(soc)->memb[soc->token].sess;
	if(
		sess->op_lock == MMN_SRV_OP_LOCK_WRITE
		&& sess->operating_on == soc->req_state_write_want_info.recipient
	) return;
	sess->op_lock = MMN_SRV_OP_LOCK_WRITE;
	sess->operating_on = soc->req_state_write_want_info.recipient;
	write_state(soc, 1, &soc->req_state_write_free_space, req_5_state_cb);
}

static void req_4_state_cb(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	mmn_srv_buf_t * buf = get_buf(srv, soc->token, soc->req_state_write_want_info.recipient);
	soc->req_state_write_free_space = srv->buf_size - buf->len;
	req_5_state(soc);
}
static void req_4_state(mmn_srv_socket_t * soc)
{
	read_state(soc, sizeof(soc->req_state_write_want_info), (uint8_t *) &soc->req_state_write_want_info, req_4_state_cb);
}

static void req_3_state_cb(mmn_srv_socket_t * soc)
{
	if(soc->req_3_state_op == MMN_SRV_OPCODE_WRITE) {
		req_4_state(soc);
	}
	else if(soc->req_3_state_op == MMN_SRV_OPCODE_READ) {
		req_7_state(soc);
	}
	else if(soc->req_3_state_op == MMN_SRV_OPCODE_SET_INTEREST) {
		req_10_state(soc);
	}
	else if(soc->req_3_state_op == MMN_SRV_OPCODE_POLL) {
		req_11_state(soc);
	}
	else if(soc->req_3_state_op == MMN_SRV_OPCODE_CROSSPOINT) {
		req_13_state(soc);
	}
	else if(soc->req_3_state_op == MMN_SRV_OPCODE_WHEREAMI) {
		req_15_state(soc);
	}
}
static void req_3_state(mmn_srv_socket_t * soc)
{
	srv_from_soc(soc)->memb[soc->token].sess.op_lock = MMN_SRV_OP_LOCK_NONE;
	read_state(soc, 1, &soc->req_3_state_op, req_3_state_cb);
}

static void req_2_state_cb(mmn_srv_socket_t * soc)
{
	req_3_state(soc);
}
static void req_2_state(mmn_srv_socket_t * soc)
{
	write_state(soc, 1, &soc->token, req_2_state_cb);
}

static void req_1_state_cb(mmn_srv_socket_t * soc)
{
	mmn_srv_t * srv = srv_from_soc(soc);
	if(soc->req_1_state_byte == 255) {
		soc->token = srv->token_counter++;
		poll_notify(srv, 255);
		req_2_state(soc);
	} else {
		soc->token = soc->req_1_state_byte;
		if(soc->token >= srv->token_counter) return;
		req_3_state(soc);
	}
}
static void req_1_state(mmn_srv_socket_t * soc)
{
	read_state(soc, 1, &soc->req_1_state_byte, req_1_state_cb);
}

void mmn_srv_init(
	mmn_srv_t * srv,
	uint8_t socket_count,
	uint8_t buf_size,
	uint8_t * aux_memory,
	const mmn_srv_cbs_t * cbs
) {
	srv->socket_count = socket_count;
	srv->token_counter = 0;
	srv->buf_size = buf_size;
	srv->crosspoint_is_transferring = false;
	srv->xpoint_isr_ctx.progress = 0;
	srv->bufs = aux_memory;
	uint8_t * flags_memory = aux_memory + ((sizeof(mmn_srv_buf_t) + buf_size) * socket_count * socket_count);
	srv->flags = (mmn_srv_flags_t *) flags_memory;
	srv->cbs = cbs;

	for(uint8_t i = 0; i < socket_count; i++) {
		for(uint8_t j = 0; j < socket_count; j++) {
			mmn_srv_buf_t * buf = get_buf(srv, i, j);
			buf->len = 0;
			buf->next_out = 0;
			mmn_srv_flags_t * flags = get_flags(srv, i, j);
			flags->interests = 0;
			flags->ready = 0;
		}
	}
}

void mmn_srv_member_init(
	mmn_srv_t * srv,
	mmn_srv_member_t * memb,
	uint8_t socket_index,
	void * ctx
) {
	memb->soc.index = socket_index;
	memb->soc.ctx = ctx;

	memb->soc.is_transferring = false;
	mbb_srv_init(&memb->soc.bb, &srv->cbs->pin_cbs, ctx);

	memb->soc.token = 255;

	memb->sess.op_lock = MMN_SRV_OP_LOCK_NONE;
	memb->sess.poll_reported_token_count = 0;
	memb->sess.poll_next_check = 0;
	memb->sess.poll_blocked = false;

	memb->soc.blocked_on = MMN_SRV_BLOCKED_ON_FAULT;
	req_1_state(&memb->soc);
}

void mmn_srv_timer_isr_handler(mmn_srv_t * srv)
{
	uint8_t socket_count = srv->socket_count;
	for(uint8_t i = 0; i < socket_count; i++) {
		mmn_srv_socket_t * soc = &srv->memb[i].soc;
		mbb_srv_transfer_t trn;
		if(!soc->is_transferring && srv->cbs->get_trn(soc->ctx, &trn)) {
			soc->is_transferring = true;
			mbb_srv_start_byte_transfer(&soc->bb, trn);
		}
		if(soc->is_transferring && mbb_srv_continue_byte_transfer(&soc->bb)) {
			soc->is_transferring = false;
			srv->cbs->set_trn_done(soc->ctx, mbb_srv_get_read_byte(&soc->bb));
		}
	}

	switch (srv->xpoint_isr_ctx.progress) {
		case 0:
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_CLK, false);
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_CLEAR, true);
			srv->xpoint_isr_ctx.progress += 1;
			break;
		case 1:
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_CLK, true);
			srv->xpoint_isr_ctx.progress += 1;
			break;
		case 2: {
			if(!srv->cbs->get_xpoint(srv, &srv->xpoint_isr_ctx.command)) break;
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_CLEAR, false);
			uint32_t edge_point_count = 4 * srv->socket_count;
			srv->xpoint_isr_ctx.bits_left = bits_needed_to_represent_value((edge_point_count + 1) * edge_point_count - 1) + 1;
			srv->xpoint_isr_ctx.progress += 1;
			/* fallthrough */
		}
		case 3:
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_CLK, false);
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_DAT, srv->xpoint_isr_ctx.command & 1);
			srv->xpoint_isr_ctx.command >>= 1;
			srv->xpoint_isr_ctx.bits_left -= 1;
			srv->xpoint_isr_ctx.progress += 1;
			break;
		case 4:
			srv->cbs->xpoint_pin_write(srv, MMN_SRV_XPOINT_PIN_CLK, true);
			if(srv->xpoint_isr_ctx.bits_left) {
				srv->xpoint_isr_ctx.progress = 3;
			} else {
				srv->cbs->set_xpoint_done(srv);
				srv->xpoint_isr_ctx.progress = 0;
			}
			break;
	}
}

void mmn_srv_main_loop_handler(mmn_srv_t * srv)
{
	uint8_t socket_count = srv->socket_count;
	bool something_happened;
	do {
		something_happened = false;
		for(uint8_t i = 0; i < socket_count; i++) {
			mmn_srv_socket_t * soc = &srv->memb[i].soc;
			switch(soc->blocked_on) {
				case MMN_SRV_BLOCKED_ON_TRANSFER:
					if(!srv->cbs->get_trn_done(soc->ctx, &soc->read_byte)) continue;
					break;
				case MMN_SRV_BLOCKED_ON_POLL:
					if(srv->memb[soc->token].sess.poll_blocked && !time_is_up(soc)) continue;
					srv->memb[soc->token].sess.poll_blocked = false;
					break;
				case MMN_SRV_BLOCKED_ON_XPOINT:
					if(!srv->cbs->get_xpoint_done(srv)) continue;
					break;
				default:
					continue;
			}
			something_happened = true;
			soc->blocked_on = MMN_SRV_BLOCKED_ON_FAULT;
			soc->state_machine_cb(soc);
		}
	} while(something_happened);
}
