#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"
#include "../mcp_module_stm32/mcp_module_stm32_mcp_fs.h"

#include "../mcp_assert.h"

#include "main_4th_gz.h"

#if FLASH_PAGE_SIZE != 2048
#error flash pages are not the expected size
#endif

#define APP_FS_BLOCK_COUNT 1

extern const unsigned char fs_bin[];

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_8},
    {GPIOA, GPIO_PIN_12}
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"eth0\"}\n", sizeof("{\"name\":\"eth0\"}\n") - 1},
    {"main.4th.gz", main_4th_gz, sizeof(main_4th_gz)},
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);
}

void app_main(void) {
    int res;

    LOGLN("start eth0");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);

    static mcp_module_stm32_mcp_fs_mini_t mmfs;
    static uint8_t mmfs_aligned_aux_memory[MCP_MODULE_STM32_MCP_FS_MINI_AUX_MEMORY_SIZE] __attribute__((aligned));

    res = mcp_module_stm32_mcp_fs_mini_init(&mmfs, mmfs_aligned_aux_memory, ((uintptr_t)fs_bin - FLASH_BASE) / FLASH_PAGE_SIZE, APP_FS_BLOCK_COUNT);
    assert(res == 0);

    mcp_module_stm32_run(
        clk_dat_pins,
        &MICROSECOND_TIMER,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        &mmfs,
        &mcp_module_stm32_mcp_fs_mini_rw_fs_vtable,
        NULL,
        driver_protocol_cb
    );
}
