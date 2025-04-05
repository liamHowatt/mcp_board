#include "mcp_module.h"
#include "../digest/digest/sha2.h"

#include <string.h>
#include <assert.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define peer_read  mcp_module_driver_read
#define peer_write mcp_module_driver_write

typedef struct {
    mbb_cli_t bb;
    void * uctx;
    mcp_module_delay_us_cb_t delay_us_cb;
    mcp_module_wait_clk_high_cb_t wait_clk_high_cb;
    uint8_t whereami;
    uint16_t us_delay;
} ctx_t;

struct mcp_module_driver_handle {
    ctx_t ctx;
    uint8_t token;
};
typedef struct mcp_module_driver_handle cwt_t;

static void run1(ctx_t * ctx)
{
    mbb_cli_status_t status;
    while (MBB_CLI_STATUS_DONE != (status = mbb_cli_continue_byte_transfer(&ctx->bb))) {
        switch (status) {
            case MBB_CLI_STATUS_DO_DELAY:
                ctx->delay_us_cb(ctx->uctx, ctx->us_delay);
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

static uint32_t begin_read_or_write(cwt_t * cwt, bool is_read, uint32_t size)
{
    write1(&cwt->ctx, is_read);
    write1(&cwt->ctx, MIN(size, 255));
    write1(&cwt->ctx, cwt->token);
    uint32_t internal_buffer_size = read1(&cwt->ctx);
    return MIN(size, internal_buffer_size);
}

void mcp_module_driver_read(cwt_t * cwt, void * dst, uint32_t size)
{
    uint8_t * dstu8 = dst;
    while(size) {
        uint32_t readable_count = begin_read_or_write(cwt, true, size);
        size -= readable_count;
        while(readable_count--) {
            *dstu8++ = read1(&cwt->ctx);
        }
    }
}

void mcp_module_driver_write(cwt_t * cwt, const void * src, uint32_t size)
{
    const uint8_t * srcu8 = src;
    while(size) {
        uint32_t free_space = begin_read_or_write(cwt, false, size);
        size -= free_space;
        while(free_space--) {
            write1(&cwt->ctx, *srcu8++);
        }
    }
}

static void peer_write_zeros(cwt_t * cwt, uint32_t count)
{
    while(count) {
        uint32_t free_space = begin_read_or_write(cwt, false, count);
        count -= free_space;
        while(free_space--) {
            write1(&cwt->ctx, 0);
        }
    }
}

static void peer_read_null(cwt_t * cwt, uint32_t count)
{
    while(count) {
        uint32_t readable_count = begin_read_or_write(cwt, true, count);
        count -= readable_count;
        while(readable_count--) {
            read1(&cwt->ctx);
        }
    }
}

static uint8_t peer_read1(cwt_t * cwt)
{
    uint8_t b;
    peer_read(cwt, &b, 1);
    return b;
}

static void peer_write1(cwt_t * cwt, uint8_t b)
{
    peer_write(cwt, &b, 1);
}

static int try_peer_read1(cwt_t * cwt)
{
    if(0 == begin_read_or_write(cwt, true, 1)) return -1;
    return read1(&cwt->ctx);
}

static void file_list_counter(void * vctx, const char * fname)
{
    uint32_t * fname_byte_count = vctx;
    *fname_byte_count += strlen(fname) + 1;
}

static void file_list_writer(void * vctx, const char * fname)
{
    cwt_t * cwt = vctx;
    peer_write(cwt, fname, strlen(fname) + 1);
}

uint8_t mcp_module_driver_whereami(cwt_t * cwt)
{
    if(cwt->ctx.whereami != 255) return cwt->ctx.whereami;

    write1(&cwt->ctx, 5); // whereami
    uint8_t whereami = read1(&cwt->ctx);
    cwt->ctx.whereami = whereami;
    return whereami;
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
    const mcp_module_rw_fs_vtable_t * rw_fs_vtable,
    void * driver_protocol_ctx,
    mcp_module_driver_protocol_cb_t driver_protocol_cb
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

    cwt_t cwt;

    mbb_cli_init(&cwt.ctx.bb, bb_read_cb, bb_write_cb, uctx);
    cwt.ctx.uctx = uctx;
    cwt.ctx.delay_us_cb = delay_us_cb;
    cwt.ctx.wait_clk_high_cb = wait_clk_high_cb;
    cwt.ctx.whereami = 255;
    cwt.ctx.us_delay = 500;

    write1(&cwt.ctx, 255);
    read1(&cwt.ctx); // read our token

    write1(&cwt.ctx, 6); // MMN_SRV_OPCODE_GETINFO
    uint8_t info_count = read1(&cwt.ctx);
    if(info_count > 0) {
        uint8_t us = read1(&cwt.ctx);
        if(us != 255) {
            cwt.ctx.us_delay = us;
        }
        while(--info_count) read1(&cwt.ctx);
    }

    while(1) {
        write1(&cwt.ctx, 3); // poll
        write1(&cwt.ctx, 255); // delay forever
        uint8_t flags = read1(&cwt.ctx);

        assert(flags);

        uint8_t token_count = 0;
        bool has_readable = false;

        if(flags & 0x4) {
            token_count = read1(&cwt.ctx);
        }
        if(flags & 0x1) {
            has_readable = true;
            cwt.token = read1(&cwt.ctx);
        }

        for(uint8_t i = 0; i < token_count; i++) {
            write1(&cwt.ctx, 2); // set interest
            write1(&cwt.ctx, i); // token
            write1(&cwt.ctx, 0x1); // interested in reading
        }

        if(!has_readable) continue;

        int protocol;
        while((protocol = try_peer_read1(&cwt)) >= 0) {
            char fname[256];
            if(protocol == 0) { // file protocol
                peer_write1(&cwt, 0); // we support it
                uint8_t action = peer_read1(&cwt);
                if(action == 2) { // list
                    uint32_t fname_byte_count = 0;
                    for(uint32_t i = 0; i < static_file_table_size; i++)
                        fname_byte_count += strlen(static_file_table[i].name) + 1;
                    uint8_t fs_res = 0;
                    if(rw_fs_vtable) {
                        uint32_t fname_byte_count_save = fname_byte_count;
                        fs_res = rw_fs_vtable->list(rw_fs_ctx, &fname_byte_count, file_list_counter);
                        if(fs_res) {
                            fname_byte_count = fname_byte_count_save;
                        }
                    }
                    peer_write(&cwt, &fname_byte_count, sizeof(fname_byte_count));
                    for(uint32_t i = 0; i < static_file_table_size; i++)
                        peer_write(&cwt, static_file_table[i].name, strlen(static_file_table[i].name) + 1);
                    if(rw_fs_vtable && !fs_res) {
                        fs_res = rw_fs_vtable->list(rw_fs_ctx, &cwt, file_list_writer);
                        // can't recover from this because we've already
                        // promised `fname_byte_count` bytes of file names
                        assert(!fs_res);
                    }
                }
                else if (action == 0 || action == 1) { // write or read
                    uint8_t buf[256];

                    uint8_t fname_len = peer_read1(&cwt);
                    peer_read(&cwt, fname, fname_len);
                    fname[fname_len] = '\0';

                    uint32_t i;
                    for(i = 0; i < static_file_table_size; i++)
                        if(0 == strcmp(fname, static_file_table[i].name)) break;
                    if(i < static_file_table_size) {
                        if(action == 0) { // write not allowed on static file
                            peer_write1(&cwt, 2); // EACCES
                            continue;
                        }
                        peer_write1(&cwt, 0); // OK
                        const mcp_module_static_file_table_entry_t * entry = static_file_table + i;
                        const uint8_t * content_u8 = entry->content;
                        uint32_t remain = entry->content_size;
                        while(1) {
                            uint8_t sub_action = peer_read1(&cwt);
                            if(sub_action == 0) { // continue write or read
                                uint32_t read_len;
                                peer_read(&cwt, &read_len, 4);
                                uint32_t actually_read = MIN(read_len, remain);
                                peer_write(&cwt, &actually_read, 4);
                                peer_write(&cwt, content_u8, actually_read);
                                if(actually_read && actually_read < read_len) peer_write_zeros(&cwt, 5);
                                else peer_write1(&cwt, 0); // OK
                                content_u8 += actually_read;
                                remain -= actually_read;
                            }
                            else if(sub_action == 1) { // close
                                peer_write1(&cwt, 0); // OK
                                break;
                            }
                            else if(sub_action == 2) { // fstat
                                buf[0] = 0; // OK
                                uint16_t mode = 0444;
                                memcpy(buf + 1, &mode, 2);
                                memcpy(buf + 3, &entry->content_size, 4);
                                uint16_t blksize = 1024;
                                memcpy(buf + 7, &blksize, 2);
                                peer_write(&cwt, buf, 9);
                            }
                            else assert(0);
                        }
                        continue;
                    }

                    if(!rw_fs_vtable) {
                        peer_write1(&cwt, action ? MCP_MODULE_RW_FS_RESULT_ENOENT : 6); // read:ENOENT, write:EROFS
                        continue;
                    }

                    uint8_t fs_res = rw_fs_vtable->open(rw_fs_ctx, action, fname);
                    if(fs_res != MCP_MODULE_RW_FS_RESULT_OK) {
                        peer_write1(&cwt, fs_res);
                        continue;
                    }
                    peer_write1(&cwt, 0); // OK
                    uint32_t file_pos = 0;
                    while(1) {
                        uint8_t sub_action = peer_read1(&cwt);
                        if(sub_action == 0) { // continue write or read
                            uint32_t remain;
                            peer_read(&cwt, &remain, 4);
                            if(action == 0) { // write
                                if(fs_res) {
                                    fs_res = MCP_MODULE_RW_FS_RESULT_EIO;
                                } else {
                                    while(remain) {
                                        uint32_t chunk = MIN(remain, 256);
                                        peer_read(&cwt, buf, chunk);
                                        fs_res = rw_fs_vtable->write(rw_fs_ctx, buf, chunk);
                                        if(fs_res) break;
                                        remain -= chunk;
                                        file_pos += chunk;
                                    }
                                }
                                peer_read_null(&cwt, remain);
                                peer_write1(&cwt, fs_res);
                            }
                            else { // read
                                bool zero_chunk_sent = false;
                                if(fs_res) {
                                    fs_res = MCP_MODULE_RW_FS_RESULT_EIO;
                                } else {
                                    while(remain) {
                                        uint32_t try = MIN(remain, 256);
                                        uint32_t actually_read;
                                        fs_res = rw_fs_vtable->read(rw_fs_ctx, buf, try, &actually_read);
                                        if(fs_res) break;
                                        peer_write(&cwt, &actually_read, 4);
                                        peer_write(&cwt, buf, actually_read);
                                        remain -= actually_read;
                                        file_pos += actually_read;
                                        if(actually_read < try) {
                                            if(actually_read == 0) zero_chunk_sent = true;
                                            break;
                                        }
                                    }
                                }
                                if(!zero_chunk_sent && remain) {
                                    peer_write_zeros(&cwt, 4);
                                }
                                peer_write1(&cwt, fs_res);
                            }
                        }
                        else if(sub_action == 1) { // close
                            if(!fs_res) {
                                peer_write1(&cwt, rw_fs_vtable->close(rw_fs_ctx));
                            } else {
                                peer_write1(&cwt, 0);
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
                            peer_write(&cwt, buf, 9);
                        }
                        else assert(0);
                    }
                }
                else if(action == 3) { // delete
                    uint8_t fname_len = peer_read1(&cwt);
                    peer_read(&cwt, fname, fname_len);
                    fname[fname_len] = '\0';

                    uint32_t i;
                    for(i = 0; i < static_file_table_size; i++)
                        if(0 == strcmp(fname, static_file_table[i].name)) break;
                    if(i < static_file_table_size) {
                        peer_write1(&cwt, 2); // EACCES
                        continue;
                    }

                    peer_write1(&cwt, rw_fs_vtable->delete(rw_fs_ctx, fname));
                }
                else assert(0);
            }
            else if(protocol == 1) { // driver protocol
                if(driver_protocol_cb) {
                    peer_write1(&cwt, 0); // we support it
                    driver_protocol_cb(&cwt, driver_protocol_ctx);
                }
                else {
                    peer_write1(&cwt, 1); // we don't support it
                }
            }
            else if(protocol == 2) { // hash protocol
                peer_write1(&cwt, 0); // we support it
                uint8_t fname_len = peer_read1(&cwt);
                peer_read(&cwt, fname, fname_len);
                fname[fname_len] = '\0';

                struct sha256_state state = sha256_init;
                union digest_state * u = (union digest_state *) &state;
                uint8_t block[64];
                uint8_t hash[32];

                uint32_t i;
                for(i = 0; i < static_file_table_size; i++)
                    if(0 == strcmp(fname, static_file_table[i].name)) break;
                if(i < static_file_table_size) {
                    peer_write1(&cwt, 0); // OK

                    uint32_t remain = static_file_table[i].content_size;
                    const uint8_t * p = static_file_table[i].content;

                    while(remain >= 64) {
                        sha256_block(u, p);
                        p += 64;
                        remain -= 64;
                    }
                    memcpy(block, p, remain); /* because `sha256_final` uses `block` as scratch */
                    sha256_final(u, block, remain, hash);

                    peer_write(&cwt, hash, 32);

                    continue;
                }

                if(!rw_fs_vtable) {
                    peer_write1(&cwt, MCP_MODULE_RW_FS_RESULT_ENOENT);
                    continue;
                }

                uint8_t fs_res;

                fs_res = rw_fs_vtable->open(rw_fs_ctx, true, fname);
                if(fs_res) {
                    peer_write1(&cwt, fs_res);
                    continue;
                }

                uint32_t actually_read;
                while(1) {
                    fs_res = rw_fs_vtable->read(rw_fs_ctx, block, 64, &actually_read);
                    if(fs_res || actually_read < 64) {
                        break;
                    }
                    sha256_block(u, block);
                }

                if(fs_res) {
                    peer_write1(&cwt, fs_res);
                    continue;
                }

                fs_res = rw_fs_vtable->close(rw_fs_ctx);

                peer_write1(&cwt, fs_res);

                if(fs_res) {
                    continue;
                }

                sha256_final(u, block, actually_read, hash);

                peer_write(&cwt, hash, 32);
            }
            else {
                peer_write1(&cwt, 1); // we don't support it
            }
        }
    }
}
