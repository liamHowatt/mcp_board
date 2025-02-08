#pragma once

#include "mcp_modnet_server_bitbang_include.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MMN_SRV_AUX_MEMORY_SIZE(soc_count, buf_size) ((sizeof(mmn_srv_buf_t) + buf_size + sizeof(mmn_srv_flags_t)) * soc_count * soc_count)

#define MMN_SRV_FLAG_READABLE    0x01
#define MMN_SRV_FLAG_WRITABLE    0x02
#define MMN_SRV_FLAG_PRESENCE    0x04

typedef uint32_t mmn_srv_crosspoint_command_t;

typedef enum {
	MMN_SRV_OPCODE_WRITE = 0,
	MMN_SRV_OPCODE_READ,
	MMN_SRV_OPCODE_SET_INTEREST,
	MMN_SRV_OPCODE_POLL,
	MMN_SRV_OPCODE_CROSSPOINT,
	MMN_SRV_OPCODE_WHEREAMI
} mmn_srv_opcode_t;

typedef enum {
	MMN_SRV_XPOINT_PIN_CLK = 0,
	MMN_SRV_XPOINT_PIN_DAT,
	MMN_SRV_XPOINT_PIN_CLEAR
} mmn_srv_xpoint_pin_t;

typedef struct mmn_srv_t mmn_srv_t;
typedef struct mmn_srv_socket_t mmn_srv_socket_t;

typedef uint32_t (*mmn_srv_get_tick_ms_cb_t)(mmn_srv_t *);
typedef void (*mmn_srv_isr_set_transfer_cb_t)(void *, mbb_srv_transfer_t trn);
typedef bool (*mmn_srv_isr_get_transfer_cb_t)(void *, mbb_srv_transfer_t * dst);
typedef void (*mmn_srv_isr_set_transfer_done_cb_t)(void *, uint8_t read_byte);
typedef bool (*mmn_srv_isr_get_transfer_done_cb_t)(void *, uint8_t * read_byte_dst);
typedef void (*mmn_srv_set_crosspoint_command_cb_t)(mmn_srv_t *, mmn_srv_crosspoint_command_t command);
typedef bool (*mmn_srv_get_crosspoint_command_cb_t)(mmn_srv_t *, mmn_srv_crosspoint_command_t * command_dst);
typedef void (*mmn_srv_set_crosspoint_command_done_cb_t)(mmn_srv_t *);
typedef bool (*mmn_srv_get_crosspoint_command_done_cb_t)(mmn_srv_t *);
typedef void (*mmn_srv_xpoint_pin_write_cb_t)(mmn_srv_t *, mmn_srv_xpoint_pin_t, bool);

typedef void (*mmn_srv_state_machine_cb_t)(mmn_srv_socket_t *);

typedef struct {
	mmn_srv_get_tick_ms_cb_t get_tick_ms;
	mmn_srv_isr_set_transfer_cb_t set_trn;
	mmn_srv_isr_get_transfer_cb_t get_trn;
	mmn_srv_isr_set_transfer_done_cb_t set_trn_done;
	mmn_srv_isr_get_transfer_done_cb_t get_trn_done;
	mbb_srv_cbs_t pin_cbs;
	mmn_srv_set_crosspoint_command_cb_t set_xpoint;
	mmn_srv_get_crosspoint_command_cb_t get_xpoint;
	mmn_srv_set_crosspoint_command_done_cb_t set_xpoint_done;
	mmn_srv_get_crosspoint_command_done_cb_t get_xpoint_done;
	mmn_srv_xpoint_pin_write_cb_t xpoint_pin_write;
} mmn_srv_cbs_t;

typedef struct {
	uint8_t len;
	uint8_t next_out;
	uint8_t data[];
} mmn_srv_buf_t;

typedef struct {
	uint8_t interests;
	uint8_t ready;
} mmn_srv_flags_t;

typedef struct {
	uint32_t command;
	uint8_t progress;
	uint8_t bits_left;
}mmn_srv_xpoint_isr_ctx_t ;

struct mmn_srv_socket_t {
	// const (shared with isr)
	void * ctx;
	uint8_t index;

	// isr
	bool is_transferring;
	mbb_srv_t bb;

	// main loop
	mmn_srv_state_machine_cb_t state_machine_cb;
	uint8_t blocked_on;
	
	// blocking results
	uint8_t read_byte;

	// shared between states
	uint8_t token;

	// read/write state
	union {
		struct {
			mmn_srv_state_machine_cb_t state_read_then_cb;
			uint8_t * state_read_dst;
			size_t state_read_i;
			size_t state_read_len;
		};
		struct {
			mmn_srv_state_machine_cb_t state_write_then_cb;
			const uint8_t * state_write_src;
			size_t state_write_i;
			size_t state_write_len;
		};
	};

	// req states
	union {
		struct {
			struct {
				uint8_t amount;
				uint8_t recipient;
			} req_state_write_want_info;
			uint8_t req_state_write_free_space;
		};
		struct {
			struct {
				uint8_t amount;
				uint8_t from;
			} req_state_read_want_info;
			uint8_t req_state_read_available_data;
		};
		struct {
			uint32_t req_state_poll_start_time;
			uint8_t req_state_poll_timeout;
		};
	};
	union {
		struct {
			uint8_t req_1_state_byte;
		};
		struct {
			uint8_t req_3_state_op;
		};
		struct {
			uint8_t req_6_state_written_dst;
			uint8_t req_6_state_i;
		};
		struct {
			uint8_t req_9_state_reading_src;
			uint8_t req_9_state_i;
		};
		struct {
			struct {
				uint8_t session;
				uint8_t flags;
			} req_10_state_intr_set;
		};
		struct {
			uint8_t req_12_state_event_data[3];
		};
		struct {
			struct {
				uint8_t input;
				uint8_t output;
				uint8_t pin_info;
			} req_13_state_xpoint_data;
		};
	};
};

typedef struct {
	uint8_t op_lock;
	uint8_t operating_on;
	uint8_t poll_reported_token_count;
	uint8_t poll_next_check;
	bool poll_blocked;
} mmn_srv_session_t;

typedef struct {
	mmn_srv_socket_t soc;
	mmn_srv_session_t sess;
} mmn_srv_member_t;

struct mmn_srv_t {
	uint8_t socket_count;
	uint8_t token_counter;
	uint8_t buf_size;
	bool crosspoint_is_transferring;
	mmn_srv_xpoint_isr_ctx_t xpoint_isr_ctx;
	uint8_t * bufs;
	mmn_srv_flags_t * flags;
	const mmn_srv_cbs_t * cbs;
	mmn_srv_member_t memb[];
};

void mmn_srv_init(
	mmn_srv_t * srv,
	uint8_t socket_count,
	uint8_t buf_size,
	uint8_t * aux_memory,
	const mmn_srv_cbs_t * cbs
);
void mmn_srv_member_init(
	mmn_srv_t * srv,
	mmn_srv_member_t * memb,
	uint8_t socket_index,
	void * ctx
);
void mmn_srv_timer_isr_handler(mmn_srv_t * srv);
void mmn_srv_main_loop_handler(mmn_srv_t * srv);
