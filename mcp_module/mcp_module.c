#include "mcp_module.h"

#include <string.h>
#include <assert.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    mbb_cli_t bb;
    void * uctx;
    mcp_module_delay_us_cb_t delay_us_cb;
    mcp_module_wait_clk_high_cb_t wait_clk_high_cb;
} ctx_t;

typedef struct {
    ctx_t * ctx;
    uint8_t token;
} file_list_writer_t;

static void run1(ctx_t * ctx)
{
    mbb_cli_status_t status;
    while (MBB_CLI_STATUS_DONE != (status = mbb_cli_continue_byte_transfer(&ctx->bb))) {
        switch (status) {
            case MBB_CLI_STATUS_DO_DELAY:
                ctx->delay_us_cb(ctx->uctx, 500);
                break;
            case MBB_CLI_STATUS_WAIT_CLK_PIN_HIGH:
                ctx->wait_clk_high_cb(ctx->uctx);
                break;
            default:
                assert(0);
        }
    }
}

static uint8_t read1(ctx_t * ctx)
{
    mbb_cli_start_byte_transfer(&ctx->bb, MBB_CLI_BYTE_TRANSFER_READ);
    run1(ctx);
    return mbb_cli_get_read_byte(&ctx->bb);
}

static void write1(ctx_t * ctx, uint8_t data)
{
    mbb_cli_start_byte_transfer(&ctx->bb, MBB_CLI_BYTE_TRANSFER_WRITE(data));
    run1(ctx);
}

static uint32_t begin_read_or_write(ctx_t * ctx, bool is_read, uint8_t peer, uint32_t size)
{
    write1(ctx, is_read);
    write1(ctx, MIN(size, 255));
    write1(ctx, peer);
    uint32_t internal_buffer_size = read1(ctx);
    return MIN(size, internal_buffer_size);
}

static void peer_read(ctx_t * ctx, uint8_t peer, void * dst, uint32_t size)
{
    uint8_t * dstu8 = dst;
    while(size) {
        uint32_t readable_count = begin_read_or_write(ctx, true, peer, size);
        size -= readable_count;
        while(readable_count--) {
            *dstu8++ = read1(ctx);
        }
    }
}

static void peer_write(ctx_t * ctx, uint8_t peer, const void * src, uint32_t size)
{
    const uint8_t * srcu8 = src;
    while(size) {
        uint32_t free_space = begin_read_or_write(ctx, false, peer, size);
        size -= free_space;
        while(free_space--) {
            write1(ctx, *srcu8++);
        }
    }
}

static void peer_write_zeros(ctx_t * ctx, uint8_t peer, uint32_t count)
{
    while(count) {
        uint32_t free_space = begin_read_or_write(ctx, false, peer, count);
        count -= free_space;
        while(free_space--) {
            write1(ctx, 0);
        }
    }
}

static void peer_read_null(ctx_t * ctx, uint8_t peer, uint32_t count)
{
    while(count) {
        uint32_t readable_count = begin_read_or_write(ctx, true, peer, count);
        count -= readable_count;
        while(readable_count--) {
            read1(ctx);
        }
    }
}

static uint8_t peer_read1(ctx_t * ctx, uint8_t peer)
{
    uint8_t b;
    peer_read(ctx, peer, &b, 1);
    return b;
}

static void peer_write1(ctx_t * ctx, uint8_t peer, uint8_t b)
{
    peer_write(ctx, peer, &b, 1);
}

static int try_peer_read1(ctx_t * ctx, uint8_t peer)
{
    if(0 == begin_read_or_write(ctx, true, peer, 1)) return -1;
    return read1(ctx);
}

static void file_list_counter(void * vctx, const char * fname)
{
    uint32_t * fname_byte_count = vctx;
    *fname_byte_count += strlen(fname) + 1;
}

static void file_list_writer(void * vctx, const char * fname)
{
    file_list_writer_t * flw = vctx;
    peer_write(flw->ctx, flw->token, fname, strlen(fname) + 1);
}

void mcp_module_run(
    void * uctx,
    mbb_cli_read_cb_t bb_read_cb,
    mbb_cli_write_cb_t bb_write_cb,
    mcp_module_delay_us_cb_t delay_us_cb,
    mcp_module_wait_clk_high_cb_t wait_clk_high_cb,
    const mcp_module_static_file_table_entry_t * static_file_table,
    uint32_t static_file_table_size,
    void * rw_fs_ctx,
    const mcp_module_rw_fs_vtable_t * rw_fs_vtable
)
{
    delay_us_cb(uctx, 100 * 1000); // give the base time to start

    bb_write_cb(uctx, MBB_CLI_PIN_CLK, 1);
    bb_write_cb(uctx, MBB_CLI_PIN_DAT, 1);

    while(!bb_read_cb(uctx, MBB_CLI_PIN_CLK) || !bb_read_cb(uctx, MBB_CLI_PIN_DAT));
    delay_us_cb(uctx, 500);
    bb_write_cb(uctx, MBB_CLI_PIN_CLK, 0);
    delay_us_cb(uctx, 500);
    bb_write_cb(uctx, MBB_CLI_PIN_CLK, 1);
    delay_us_cb(uctx, 500);

    ctx_t ctx;

    mbb_cli_init(&ctx.bb, bb_read_cb, bb_write_cb, uctx);
    ctx.uctx = uctx;
    ctx.delay_us_cb = delay_us_cb;
    ctx.wait_clk_high_cb = wait_clk_high_cb;

    write1(&ctx, 255);
    read1(&ctx); // read our token

    while(1) {
        write1(&ctx, 3); // poll
        write1(&ctx, 255); // delay forever
        uint8_t flags = read1(&ctx);

        assert(flags);

        uint8_t token_count = 0;
        bool has_readable = false;
        uint8_t token = 255;

        if(flags & 0x4) {
            token_count = read1(&ctx);
        }
        if(flags & 0x1) {
            has_readable = true;
            token = read1(&ctx);
        }

        for(uint8_t i = 0; i < token_count; i++) {
            write1(&ctx, 2); // set interest
            write1(&ctx, i); // token
            write1(&ctx, 0x1); // interested in reading
        }

        if(!has_readable) continue;

        int protocol;
        while((protocol = try_peer_read1(&ctx, token)) >= 0) {
            if(protocol == 0) { // file protocol
                peer_write1(&ctx, token, 0); // we support it
                uint8_t action = peer_read1(&ctx, token);
                char fname[256];
                if(action == 2) { // list
                    uint32_t fname_byte_count = 0;
                    for(uint32_t i = 0; i < static_file_table_size; i++)
                        fname_byte_count += strlen(static_file_table[i].name) + 1;
                    if(rw_fs_vtable)
                        rw_fs_vtable->list(rw_fs_ctx, &fname_byte_count, file_list_counter);
                    peer_write(&ctx, token, &fname_byte_count, sizeof(fname_byte_count));
                    for(uint32_t i = 0; i < static_file_table_size; i++)
                        peer_write(&ctx, token, static_file_table[i].name, strlen(static_file_table[i].name) + 1);
                    if(rw_fs_vtable) {
                        file_list_writer_t flw = {.ctx = &ctx, .token = token};
                        rw_fs_vtable->list(rw_fs_ctx, &flw, file_list_writer);
                    }
                }
                else if (action == 0 || action == 1) { // write or read
                    uint8_t buf[256];

                    uint8_t fname_len = peer_read1(&ctx, token);
                    peer_read(&ctx, token, fname, fname_len);
                    fname[fname_len] = '\0';

                    uint32_t i;
                    for(i = 0; i < static_file_table_size; i++)
                        if(0 == strcmp(fname, static_file_table[i].name)) break;
                    if(i < static_file_table_size) {
                        if(action == 0) { // write not allowed on static file
                            peer_write1(&ctx, token, 2); // EACCES
                            continue;
                        }
                        peer_write1(&ctx, token, 0); // OK
                        const mcp_module_static_file_table_entry_t * entry = static_file_table + i;
                        const uint8_t * content_u8 = entry->content;
                        uint32_t remain = entry->content_size;
                        while(1) {
                            uint8_t sub_action = peer_read1(&ctx, token);
                            if(sub_action == 0) { // continue write or read
                                uint32_t read_len;
                                peer_read(&ctx, token, &read_len, 4);
                                uint32_t actually_read = MIN(read_len, remain);
                                peer_write(&ctx, token, content_u8, actually_read);
                                peer_write_zeros(&ctx, token, read_len - actually_read);
                                buf[0] = 0; // OK
                                memcpy(buf + 1, &actually_read, 4);
                                peer_write(&ctx, token, buf, 5);
                                content_u8 += actually_read;
                                remain -= actually_read;
                            }
                            else if(sub_action == 1) { // close
                                peer_write1(&ctx, token, 0); // OK
                                break;
                            }
                            else if(sub_action == 2) { // fstat
                                buf[0] = 0; // OK
                                uint16_t mode = 0444;
                                memcpy(buf + 1, &mode, 2);
                                memcpy(buf + 3, &entry->content_size, 4);
                                uint16_t blksize = 1024;
                                memcpy(buf + 7, &blksize, 2);
                                peer_write(&ctx, token, buf, 9);
                            }
                            else assert(0);
                        }
                        continue;
                    }

                    if(!rw_fs_vtable) {
                        peer_write1(&ctx, token, action ? MCP_MODULE_RW_FS_RESULT_ENOENT : 6); // read:ENOENT, write:EROFS
                        continue;
                    }

                    uint8_t fs_res = rw_fs_vtable->open(rw_fs_ctx, action, fname);
                    if(fs_res != MCP_MODULE_RW_FS_RESULT_OK) {
                        peer_write1(&ctx, token, fs_res);
                        continue;
                    }
                    peer_write1(&ctx, token, 0); // OK
                    uint32_t file_pos = 0;
                    while(1) {
                        uint8_t sub_action = peer_read1(&ctx, token);
                        if(sub_action == 0) { // continue write or read
                            uint32_t remain;
                            peer_read(&ctx, token, &remain, 4);
                            if(action == 0) { // write
                                if(fs_res) {
                                    fs_res = MCP_MODULE_RW_FS_RESULT_EIO;
                                } else {
                                    while(remain) {
                                        uint32_t chunk = MIN(remain, 256);
                                        peer_read(&ctx, token, buf, chunk);
                                        fs_res = rw_fs_vtable->write(rw_fs_ctx, buf, chunk);
                                        if(fs_res) break;
                                        remain -= chunk;
                                    }
                                }
                                peer_read_null(&ctx, token, remain);
                                peer_write1(&ctx, token, fs_res);
                            }
                            else { // read
                                uint32_t amt_read = 0;
                                if(fs_res) {
                                    fs_res = MCP_MODULE_RW_FS_RESULT_EIO;
                                } else {
                                    while(remain) {
                                        uint32_t try = MIN(remain, 256);
                                        uint32_t actually_read;
                                        fs_res = rw_fs_vtable->read(rw_fs_ctx, buf, try, &actually_read);
                                        if(fs_res) break;
                                        peer_write(&ctx, token, buf, actually_read);
                                        amt_read += actually_read;
                                        remain -= actually_read;
                                        if(actually_read < try) break;
                                    }
                                }
                                peer_write_zeros(&ctx, token, remain);
                                buf[0] = fs_res;
                                memcpy(buf + 1, &amt_read, 4);
                                peer_write(&ctx, token, buf, 5);
                            }
                        }
                        else if(sub_action == 1) { // close
                            if(!fs_res) {
                                peer_write1(&ctx, token, rw_fs_vtable->close(rw_fs_ctx));
                            } else {
                                peer_write1(&ctx, token, 0);
                            }
                            break;
                        }
                        else if(sub_action == 2) { // fstat
                            uint32_t size = file_pos;

                            if(fs_res) {
                                fs_res = MCP_MODULE_RW_FS_RESULT_EIO;
                            }
                            else if(action != 0) { // read
                                uint32_t actually_read;
                                while(1) {
                                    fs_res = rw_fs_vtable->read(rw_fs_ctx, buf, 256, &actually_read);
                                    if(fs_res) break;
                                    size += actually_read;
                                    if(actually_read < 256) break;
                                }
                                if(!fs_res) fs_res = rw_fs_vtable->close(rw_fs_ctx);
                                if(!fs_res) fs_res = rw_fs_vtable->open(rw_fs_ctx, true, fname);
                                if(!fs_res) {
                                    uint32_t remain = file_pos;
                                    while(remain) {
                                        uint32_t try = MIN(remain, 256);
                                        fs_res = rw_fs_vtable->read(rw_fs_ctx, buf, try, &actually_read);
                                        if(fs_res) break;
                                        assert(try == actually_read);
                                        remain -= try;
                                    }
                                }
                            }

                            buf[0] = fs_res;
                            uint16_t mode = 0666;
                            memcpy(buf + 1, &mode, 2);
                            memcpy(buf + 3, &size, 4);
                            uint16_t blksize = 1024;
                            memcpy(buf + 7, &blksize, 2);
                            peer_write(&ctx, token, buf, 9);
                        }
                        else assert(0);
                    }
                }
                else if(action == 3) { // delete
                    uint8_t fname_len = peer_read1(&ctx, token);
                    peer_read(&ctx, token, fname, fname_len);
                    fname[fname_len] = '\0';

                    uint32_t i;
                    for(i = 0; i < static_file_table_size; i++)
                        if(0 == strcmp(fname, static_file_table[i].name)) break;
                    if(i < static_file_table_size) {
                        peer_write1(&ctx, token, 2); // EACCES
                        continue;
                    }

                    peer_write1(&ctx, token, rw_fs_vtable->delete(rw_fs_ctx, fname));
                }
                else assert(0);
            }
            else {
                peer_write1(&ctx, token, 1); // we don't support it
            }
        }
    }
}
