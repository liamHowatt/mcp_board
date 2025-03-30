#include "mcp_modnet_server.h"
#include "mcp_modnet_server_bitbang_include.h"
#include "sim.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SOCKET_COUNT 12
#define BUF_SIZE 32

typedef struct {
    mmn_srv_t srv;
    mmn_srv_member_t memb[SOCKET_COUNT];
    mmn_srv_crosspoint_command_t xpoint_command;
    bool xpoint_is_transferring;
    struct sim * sim;
} test_srv_t;

typedef struct {
    // trn
    mbb_srv_transfer_t trn;
    bool is_transferring;
    // bool flip;
    bool out_of_order;

    // bb
    mbb_srv_transfer_t bb_trn;
    uint8_t read_byte;
    bool is_read;
    size_t len;
    uint8_t * data;
    bool transfer_requested_event;
} ctx_t;

static uint32_t get_tick_ms(mmn_srv_t * srv)
{
    return 0;
}

static void set_trn(void * vctx, mbb_srv_transfer_t trn)
{
    ctx_t * ctx = vctx;
    assert(!ctx->is_transferring);
    ctx->is_transferring = true;
    ctx->trn = trn;
}

static bool get_trn(void * vctx, mbb_srv_transfer_t * trn_dst)
{
    ctx_t * ctx = vctx;
    *trn_dst = ctx->trn;
    return ctx->is_transferring;
}

static void set_trn_done(void * vctx, uint8_t read_byte)
{
    ctx_t * ctx = vctx;
    assert(ctx->is_transferring);
    ctx->is_transferring = false;
    ctx->trn = read_byte;
}

static bool get_trn_done(void * vctx, uint8_t * read_byte_dst)
{
    ctx_t * ctx = vctx;
    *read_byte_dst = ctx->trn;
    return !ctx->is_transferring;
}

void set_xpoint(mmn_srv_t * srv, mmn_srv_crosspoint_command_t command)
{
    test_srv_t * tstsrv = (test_srv_t *) srv;
    assert(!tstsrv->xpoint_is_transferring);
    tstsrv->xpoint_is_transferring = true;
    tstsrv->xpoint_command = command;
}
bool get_xpoint(mmn_srv_t * srv, mmn_srv_crosspoint_command_t * command_dst)
{
    test_srv_t * tstsrv = (test_srv_t *) srv;
    *command_dst = tstsrv->xpoint_command;
    return tstsrv->xpoint_is_transferring;
}
void set_xpoint_done(mmn_srv_t * srv)
{
    test_srv_t * tstsrv = (test_srv_t *) srv;
    assert(tstsrv->xpoint_is_transferring);
    tstsrv->xpoint_is_transferring = false;
}
bool get_xpoint_done(mmn_srv_t * srv)
{
    test_srv_t * tstsrv = (test_srv_t *) srv;
    return !tstsrv->xpoint_is_transferring;
}
void xpoint_pin_write(mmn_srv_t * srv, mmn_srv_xpoint_pin_t pin, bool en)
{
    test_srv_t * tstsrv = (test_srv_t *) srv;
    enum sim_ConPin sim_pin;
    switch(pin) {
        case MMN_SRV_XPOINT_PIN_CLK:
            sim_pin = sim_ConPinClk;
            break;
        case MMN_SRV_XPOINT_PIN_DAT:
            sim_pin = sim_ConPinDat;
            break;
        case MMN_SRV_XPOINT_PIN_CLEAR:
            sim_pin = sim_ConPinClear;
            break;
        default:
            assert(0);
    }
    sim_con_pin_set(tstsrv->sim, sim_pin, en);
}

static void set_flip(void * vctx, bool flip)
{
    // ctx_t * ctx = vctx;
    // ctx->flip = flip
}
static uint32_t fpga_decode_pinno(void * vctx, uint32_t pinno)
{
    ctx_t * ctx = vctx;
    if(ctx->out_of_order) return 3 - pinno;
    return pinno;
}

static const mmn_srv_cbs_t cbs = {
    .get_tick_ms = get_tick_ms,
    .set_trn = set_trn,
    .get_trn = get_trn,
    .set_trn_done = set_trn_done,
    .get_trn_done = get_trn_done,
    .set_xpoint = set_xpoint,
	.get_xpoint = get_xpoint,
	.set_xpoint_done = set_xpoint_done,
	.get_xpoint_done = get_xpoint_done,
	.xpoint_pin_write = xpoint_pin_write,
    .set_flip = set_flip,
    .fpga_decode_pinno = fpga_decode_pinno
};

static void run_an_iteration(mmn_srv_t * srv)
{
    test_srv_t * tstsrv = (test_srv_t *) srv;
    mmn_srv_main_loop_handler(srv);
    mmn_srv_timer_isr_handler(srv);
    sim_eval(tstsrv->sim);
}

static void test_write(mmn_srv_t * srv, ctx_t * ctxs, uint8_t socket, uint8_t * src, size_t len)
{
    ctx_t * ctx = &ctxs[socket];
    ctx->is_read = false;
    ctx->len = len;
    ctx->data = src;
    while(ctx->len || !ctx->transfer_requested_event) run_an_iteration(srv);
}

static void test_read(mmn_srv_t * srv, ctx_t * ctxs, uint8_t socket, uint8_t * dst, size_t len)
{
    ctx_t * ctx = &ctxs[socket];
    ctx->is_read = true;
    ctx->len = len;
    ctx->data = dst;
    while(ctx->len || !ctx->transfer_requested_event) run_an_iteration(srv);
}

int main()
{
    static test_srv_t srv;
    srv.xpoint_is_transferring = false;

    srv.sim = sim_create();
    sim_eval(srv.sim);

    static uint8_t aux_memory[MMN_SRV_AUX_MEMORY_SIZE(SOCKET_COUNT, BUF_SIZE)];
    mmn_srv_init(&srv.srv, SOCKET_COUNT, 125, BUF_SIZE, aux_memory, &cbs);
    static ctx_t ctxs[SOCKET_COUNT] = {0};
    ctxs[9].out_of_order = true;
    for(uint8_t i = 0; i < SOCKET_COUNT; i++) {
        mmn_srv_member_init(&srv.srv, &srv.memb[i], i, &ctxs[i]);
    }

    static uint8_t buf[32];

    buf[0] = 255;
    test_write(&srv.srv, ctxs, 3, buf, 1);
    test_read(&srv.srv, ctxs, 3, buf, 1);
    printf("3 token: %d\n", (int) buf[0]);
    assert(buf[0] == 0);

    buf[0] = 255;
    test_write(&srv.srv, ctxs, 6, buf, 1);
    test_read(&srv.srv, ctxs, 6, buf, 1);
    printf("6 token: %d\n", (int) buf[0]);
    assert(buf[0] == 1);

    buf[0] = MMN_SRV_OPCODE_WRITE;
    buf[1] = 8;
    buf[2] = 1;
    test_write(&srv.srv, ctxs, 3, buf, 3);
    test_read(&srv.srv, ctxs, 3, buf, 1);
    printf("3 write free space: %d\n", (int) buf[0]);
    assert(buf[0] == 32);
    memcpy(buf, "keyboard", 8);
    test_write(&srv.srv, ctxs, 3, buf, 8);

    buf[0] = MMN_SRV_OPCODE_READ;
    buf[1] = 8;
    buf[2] = 0;
    test_write(&srv.srv, ctxs, 6, buf, 3);
    buf[9] = '\0';
    test_read(&srv.srv, ctxs, 6, buf, 9);
    printf("6 read available: %d\n", (int) buf[0]);
    printf("6 read data: %s\n", buf + 1);
    assert(0 == memcmp(buf, "\x08""keyboard", 10));

    buf[0] = MMN_SRV_OPCODE_POLL;
    buf[1] = 0;
    test_write(&srv.srv, ctxs, 3, buf, 2);
    test_read(&srv.srv, ctxs, 3, buf, 2);
    printf("flags 0x%02x (expect 0x04) presence: %d (exepct 2)\n", (int) buf[0], (int) buf[1]);
    assert(buf[0] == MMN_SRV_FLAG_PRESENCE && buf[1] == 2);

    buf[0] = MMN_SRV_OPCODE_POLL;
    buf[1] = 0;
    test_write(&srv.srv, ctxs, 3, buf, 2);
    test_read(&srv.srv, ctxs, 3, buf, 1);
    assert(buf[0] == 0);

    buf[0] = MMN_SRV_OPCODE_SET_INTEREST;
    buf[1] = 1;
    buf[2] = MMN_SRV_FLAG_READABLE | MMN_SRV_FLAG_WRITABLE;
    buf[3] = MMN_SRV_OPCODE_POLL;
    buf[4] = 0;
    test_write(&srv.srv, ctxs, 3, buf, 5);
    test_read(&srv.srv, ctxs, 3, buf, 2);
    assert(0 == memcmp(buf, "\x02""\x01", 2)); /* writable at session 1*/

    buf[0] = MMN_SRV_OPCODE_POLL;
    buf[1] = 0;
    test_write(&srv.srv, ctxs, 3, buf, 2);
    test_read(&srv.srv, ctxs, 3, buf, 1);
    assert(buf[0] == 0);

    buf[0] = MMN_SRV_OPCODE_WRITE;
    buf[1] = 33;
    buf[2] = 1;
    test_write(&srv.srv, ctxs, 3, buf, 3);
    test_read(&srv.srv, ctxs, 3, buf, 1);
    assert(buf[0] == 32);
    memset(buf, 55, 32);
    test_write(&srv.srv, ctxs, 3, buf, 32);

    buf[0] = MMN_SRV_OPCODE_POLL;
    buf[1] = 0;
    test_write(&srv.srv, ctxs, 3, buf, 2);
    test_read(&srv.srv, ctxs, 3, buf, 1);
    assert(buf[0] == 0);

    buf[0] = MMN_SRV_OPCODE_WRITE;
    buf[1] = 1;
    buf[2] = 0;
    test_write(&srv.srv, ctxs, 6, buf, 3);
    test_read(&srv.srv, ctxs, 6, buf, 1);
    assert(buf[0] == 32);
    buf[0] = 42;
    test_write(&srv.srv, ctxs, 6, buf, 1);

    buf[0] = MMN_SRV_OPCODE_READ;
    buf[1] = 1;
    buf[2] = 0;
    test_write(&srv.srv, ctxs, 6, buf, 3);
    test_read(&srv.srv, ctxs, 6, buf, 2);
    assert(buf[0] == 32);
    assert(buf[1] == 55);

    buf[0] = 255;
    test_write(&srv.srv, ctxs, 9, buf, 1);
    test_read(&srv.srv, ctxs, 9, buf, 1);
    printf("9 token: %d\n", (int) buf[0]);
    assert(buf[0] == 2);

    buf[0] = 0;
    buf[1] = MMN_SRV_OPCODE_POLL;
    buf[2] = 0;
    test_write(&srv.srv, ctxs, 4, buf, 3);
    test_read(&srv.srv, ctxs, 4, buf, 3);
    assert(buf[0] == (MMN_SRV_FLAG_PRESENCE | MMN_SRV_FLAG_READABLE | MMN_SRV_FLAG_WRITABLE));
    assert(buf[1] == 3);
    assert(buf[2] == 1);

    buf[0] = MMN_SRV_OPCODE_POLL;
    buf[1] = 0;
    test_write(&srv.srv, ctxs, 4, buf, 2);
    test_read(&srv.srv, ctxs, 4, buf, 1);
    assert(buf[0] == 0);


    // sim tests
    for(int i = 0; i < 48; i++) assert(!sim_output_pin_check(srv.sim, i));
    buf[0] = MMN_SRV_OPCODE_CROSSPOINT;
    buf[1] = 255;
    buf[2] = 0;
    buf[3] = 1;
    test_write(&srv.srv, ctxs, 3, buf, 4);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0));

    buf[0] = MMN_SRV_OPCODE_CROSSPOINT;
    buf[1] = 255;
    buf[2] = 1;
    buf[3] = 1;
    test_write(&srv.srv, ctxs, 3, buf, 4);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 4));

    buf[0] = MMN_SRV_OPCODE_CROSSPOINT;
    buf[1] = 255;
    buf[2] = 2;
    buf[3] = 0x03;
    test_write(&srv.srv, ctxs, 3, buf, 4);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 4 || i == 9));

    buf[0] = MMN_SRV_OPCODE_CROSSPOINT;
    buf[1] = 255;
    buf[2] = 1;
    buf[3] = 0;
    test_write(&srv.srv, ctxs, 3, buf, 4);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 9));

    buf[0] = MMN_SRV_OPCODE_CROSSPOINT;
    buf[1] = 2;
    buf[2] = 4;
    buf[3] = (2 << 3) | (3 << 1) | 1;
    test_write(&srv.srv, ctxs, 3, buf, 4);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 9));
    sim_input_pin_set(srv.sim, 2 * 4 + 2, true);
    sim_eval(srv.sim);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 9 || i == 4 * 4 + 3));

    buf[0] = MMN_SRV_OPCODE_CROSSPOINT;
    buf[1] = 9;
    buf[2] = 10;
    buf[3] = (0 << 3) | (0 << 1) | 1;
    test_write(&srv.srv, ctxs, 3, buf, 4);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 9 || i == 4 * 4 + 3));
    sim_input_pin_set(srv.sim, 9 * 4 + 3, true); // socket 9 is out of order so pinno 3 will be decoded to 0
    sim_eval(srv.sim);
    for(int i = 1; i < 48; i++) assert(sim_output_pin_check(srv.sim, i) == (i == 0 || i == 9 || i == 4 * 4 + 3 || i == 10 * 4 + 0));

    buf[0] = MMN_SRV_OPCODE_WHEREAMI;
    test_write(&srv.srv, ctxs, 3, buf, 1);
    buf[0] = 0;
    test_read(&srv.srv, ctxs, 3, buf, 1);
    assert(buf[0] == 3);

    buf[0] = MMN_SRV_OPCODE_GETINFO;
    test_write(&srv.srv, ctxs, 3, buf, 1);
    test_read(&srv.srv, ctxs, 3, buf, 2);
    assert(buf[0] == 1);
    assert(buf[1] == 125);
    buf[0] = MMN_SRV_OPCODE_GETINFO;
    test_write(&srv.srv, ctxs, 3, buf, 1);
    test_read(&srv.srv, ctxs, 3, buf, 2);
    assert(buf[0] == 1);
    assert(buf[1] == 125);

    sim_destroy(srv.sim);
}

void mbb_srv_init(mbb_srv_t * mbb, const mbb_srv_cbs_t * cbs, void * caller_ctx)
{
    mbb->ctx = caller_ctx;
    ctx_t * ctx = caller_ctx;
    ctx->transfer_requested_event = false;
}

void mbb_srv_start_byte_transfer(mbb_srv_t * mbb, mbb_srv_transfer_t transfer)
{
    ctx_t * ctx = mbb->ctx;
    ctx->bb_trn = transfer;
}

bool mbb_srv_continue_byte_transfer(mbb_srv_t * mbb)
{
    ctx_t * ctx = mbb->ctx;
    if(!ctx->len) {
        ctx->transfer_requested_event = true;
        return false;
    }
    ctx->transfer_requested_event = false;
    assert((ctx->bb_trn == MBB_SRV_BYTE_TRANSFER_READ) != ctx->is_read);
    if(ctx->is_read) {
        *(ctx->data++) = ctx->bb_trn;
    } else {
        ctx->read_byte = *(ctx->data++);
    }
    ctx->len -= 1;
    return true;
}

uint8_t mbb_srv_get_read_byte(mbb_srv_t * mbb)
{
    ctx_t * ctx = mbb->ctx;
    return ctx->read_byte;
}

bool mbb_srv_is_flipped(mbb_srv_t * mbb)
{
    return false;
}
