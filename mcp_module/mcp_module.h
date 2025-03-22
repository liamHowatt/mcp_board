#pragma once

#include "../mcp_bitbang/mcp_bitbang_client.h"

#include <stdint.h>

typedef struct {
    const char * name;
    const void * content;
    uint32_t content_size;
} mcp_module_static_file_table_entry_t;

typedef struct mcp_module_driver_handle mcp_module_driver_handle_t;

typedef void (*mcp_module_delay_us_cb_t)(void * hal_ctx, uint32_t us);
typedef void (*mcp_module_wait_clk_high_cb_t)(void * hal_ctx);

typedef void (*mcp_module_driver_protocol_cb_t)(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx);

typedef enum {
    MCP_MODULE_RW_FS_RESULT_OK           = 0,
    MCP_MODULE_RW_FS_RESULT_EIO          = 1,
    MCP_MODULE_RW_FS_RESULT_ENOENT       = 3,
    MCP_MODULE_RW_FS_RESULT_ENAMETOOLONG = 4,
    MCP_MODULE_RW_FS_RESULT_ENOSPC       = 5,
} mcp_module_rw_fs_result_t;

typedef struct {
    mcp_module_rw_fs_result_t (*list)(void *, void *, void (*)(void *, const char *));
    mcp_module_rw_fs_result_t (*delete)(void *, const char *);
    mcp_module_rw_fs_result_t (*open)(void *, bool is_read, const char *);
    mcp_module_rw_fs_result_t (*close)(void *);
    mcp_module_rw_fs_result_t (*read)(void *, void * dst, uint32_t size, uint32_t * actually_read);
    mcp_module_rw_fs_result_t (*write)(void *, const void * src, uint32_t size);
} mcp_module_rw_fs_vtable_t;

void mcp_module_run(
    void * hal_ctx,
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
);

void mcp_module_driver_read(mcp_module_driver_handle_t * hdl, void * dst, uint32_t size);
void mcp_module_driver_write(mcp_module_driver_handle_t * hdl, const void * src, uint32_t size);
