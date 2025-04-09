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

static const mcp_module_stm32_pin_t btn_gpios[6] = {
    {GPIOA, GPIO_PIN_11},
    {GPIOA, GPIO_PIN_8},
    {GPIOA, GPIO_PIN_7},
    {GPIOA, GPIO_PIN_6},
    {GPIOA, GPIO_PIN_0},
    {GPIOA, GPIO_PIN_12},
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"game0\"}\n", sizeof("{\"name\":\"game0\"}\n") - 1}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    bool states[6] = {1, 1, 1, 1, 1, 1};
    while(1) {
        uint8_t vals[6];
        uint8_t val_count = 0;
        for(uint8_t i = 0; i < 6; i++) {
            bool reading = HAL_GPIO_ReadPin(btn_gpios[i].port, btn_gpios[i].pin);
            if(states[i] != reading) {
                states[i] = reading;
                vals[val_count++] = (i << 1) | reading;
            }
        }
        mcp_module_driver_write(hdl, vals, val_count);
    }
}

void app_main(void) {
    int res;

    LOGLN("start game0");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);

    static mcp_module_stm32_mcp_fs_t mmfs;
    static uint8_t mmfs_aligned_aux_memory[MCP_MODULE_STM32_MCP_FS_AUX_MEMORY_SIZE(APP_FS_BLOCK_COUNT)] __attribute__((aligned));

    res = mcp_module_stm32_mcp_fs_init(&mmfs, mmfs_aligned_aux_memory, FLASH_PAGE_NB - APP_FS_BLOCK_COUNT, APP_FS_BLOCK_COUNT);
    assert(res == 0);

    mcp_module_stm32_run(
        clk_dat_pins,
        &MICROSECOND_TIMER,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        &mmfs,
        &mcp_module_stm32_mcp_fs_rw_fs_vtable,
        NULL,
        driver_protocol_cb
    );
}
