#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"
#include "../mcp_module_stm32/mcp_module_stm32_mcp_fs.h"

#include <assert.h>

#define APP_TOTAL_FLASH_SIZE (32 * 1024)
#define APP_PROGRAM_FLASH_SIZE (22 * 1024)
#define APP_FS_BLOCK_COUNT ((APP_TOTAL_FLASH_SIZE - APP_PROGRAM_FLASH_SIZE) / FLASH_PAGE_SIZE)

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_5}
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"WELL_DONE", NULL, 0},
    {"info", "one\ntwo\nthree\n", 14}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t * ctr_p = driver_protocol_ctx;

    for(int i = 0; i < 5; i++) {
        mcp_module_driver_write(hdl, ctr_p, 1);
        (*ctr_p)++;
    }
}

void app_main(void) {
    int res;

    LOGLN("start scr1");

    for(int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);
        HAL_Delay(200);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
        HAL_Delay(200);
    }

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);

    static mcp_module_stm32_mcp_fs_t mmfs;
    static uint8_t mmfs_aligned_aux_memory[MCP_MODULE_STM32_MCP_FS_AUX_MEMORY_SIZE(APP_FS_BLOCK_COUNT)] __attribute__((aligned));

    res = mcp_module_stm32_mcp_fs_init(&mmfs, mmfs_aligned_aux_memory, FLASH_PAGE_NB - APP_FS_BLOCK_COUNT, APP_FS_BLOCK_COUNT);
    assert(res == 0);

    uint8_t ctr = 0;

    mcp_module_stm32_run(
        clk_dat_pins,
        &MICROSECOND_TIMER,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        &mmfs,
        &mcp_module_stm32_mcp_fs_rw_fs_vtable,
        &ctr,
        driver_protocol_cb
    );
}
