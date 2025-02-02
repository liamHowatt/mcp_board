#include "mcp_modnet_server.h"
#include "mcp_modnet_server_bitbang_include.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SOCKET_COUNT 12
#define BUF_SIZE 32

typedef struct {
    // trn
    mbb_srv_transfer_t trn;
    bool is_transferring;

    // bb
    mbb_srv_transfer_t bb_trn;
    uint8_t read_byte;
    bool is_read;
    size_t len;
    uint8_t * data;
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

static const mmn_srv_cbs_t cbs = {
    .get_tick_ms = get_tick_ms,
    .set_trn = set_trn,
    .get_trn = get_trn,
    .set_trn_done = set_trn_done,
    .get_trn_done = get_trn_done
};

static void test_write(mmn_srv_t * srv, ctx_t * ctxs, uint8_t socket, uint8_t * src, size_t len)
{
    ctxs[socket].is_read = false;
    ctxs[socket].len = len;
    ctxs[socket].data = src;
    while(ctxs[socket].len) {
        mmn_srv_main_loop_handler(srv);
        mmn_srv_timer_isr_handler(srv);
    }
}

static void test_read(mmn_srv_t * srv, ctx_t * ctxs, uint8_t socket, uint8_t * dst, size_t len)
{
    ctxs[socket].is_read = true;
    ctxs[socket].len = len;
    ctxs[socket].data = dst;
    while(ctxs[socket].len) {
        mmn_srv_main_loop_handler(srv);
        mmn_srv_timer_isr_handler(srv);
    }
}

int main()
{
    struct {
        mmn_srv_t srv;
        mmn_srv_member_t memb[SOCKET_COUNT];
    } srv;
    uint8_t aux_memory[MMN_SRV_AUX_MEMORY_SIZE(SOCKET_COUNT, BUF_SIZE)];
    mmn_srv_init(&srv.srv, SOCKET_COUNT, BUF_SIZE, aux_memory, &cbs);
    ctx_t ctxs[SOCKET_COUNT] = {0};
    for(uint8_t i = 0; i < SOCKET_COUNT; i++) {
        mmn_srv_member_init(&srv.srv, &srv.memb[i], i, &ctxs[i]);
    }

    uint8_t buf[32];

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
}

void mbb_srv_init(mbb_srv_t * mbb, const mbb_srv_cbs_t * cbs, void * caller_ctx)
{
    mbb->ctx = caller_ctx;
}

void mbb_srv_start_byte_transfer(mbb_srv_t * mbb, mbb_srv_transfer_t transfer)
{
    ctx_t * ctx = mbb->ctx;
    ctx->bb_trn = transfer;
}

bool mbb_srv_continue_byte_transfer(mbb_srv_t * mbb)
{
    ctx_t * ctx = mbb->ctx;
    if(!ctx->len) return false;
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
