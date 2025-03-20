#pragma once

#ifndef MCP_MODULE_STM32_HAL_HEADER
#error MCP_MODULE_STM32_HAL_HEADER must be defined
#endif

#include "../mcp_module/mcp_module.h"
#include "../mcp_fs/mcp_fs.h"
#include MCP_MODULE_STM32_HAL_HEADER

#define MCP_MODULE_STM32_MCP_FS_AUX_MEMORY_SIZE(block_count) MFS_ALIGNED_AUX_MEMORY_SIZE(FLASH_PAGE_SIZE, (block_count))

typedef struct {
    mfs_t mfs;
    mfs_conf_t conf;
    uint32_t block0_page_id;
} mcp_module_stm32_mcp_fs_t;

int mcp_module_stm32_mcp_fs_init(
    mcp_module_stm32_mcp_fs_t * mmfs,
    void * aligned_aux_memory,
    int first_block_page_number,
    int block_count
);

extern const mcp_module_rw_fs_vtable_t mcp_module_stm32_mcp_fs_rw_fs_vtable;
