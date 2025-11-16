#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"
#include "../mcp_module_stm32/mcp_module_stm32_mcp_fs.h"

#include <assert.h>
#include <string.h>

#if FLASH_PAGE_SIZE != 2048
#error flash pages are not the expected size
#endif

#define APP_FS_BLOCK_COUNT 50

#define BTN_CNT 53

extern const unsigned char fs_bin[];

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOC, GPIO_PIN_3},
    {GPIOC, GPIO_PIN_2}
};

static const mcp_module_stm32_pin_t btn_gpios[BTN_CNT] = {
    {GPIOC, GPIO_PIN_14}, // U1
    {GPIOC, GPIO_PIN_15}, // U2
    {GPIOF, GPIO_PIN_0}, // ...
    {GPIOF, GPIO_PIN_1},
    {GPIOC, GPIO_PIN_0},
    {GPIOC, GPIO_PIN_1},
    {GPIOA, GPIO_PIN_2},
    {GPIOA, GPIO_PIN_3},
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_5},
    {GPIOA, GPIO_PIN_6},
    {GPIOA, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_8},
    {GPIOB, GPIO_PIN_9},
    {GPIOC, GPIO_PIN_10},
    {GPIOC, GPIO_PIN_11},
    {GPIOC, GPIO_PIN_12},
    {GPIOC, GPIO_PIN_13},
    {GPIOC, GPIO_PIN_4},
    {GPIOC, GPIO_PIN_5},
    {GPIOB, GPIO_PIN_0},
    {GPIOB, GPIO_PIN_1},
    {GPIOB, GPIO_PIN_2},
    {GPIOB, GPIO_PIN_10},
    {GPIOB, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_6},
    {GPIOB, GPIO_PIN_5},
    {GPIOB, GPIO_PIN_4},
    {GPIOB, GPIO_PIN_3},
    {GPIOD, GPIO_PIN_6},
    {GPIOA, GPIO_PIN_8},
    {GPIOB, GPIO_PIN_15},
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_13},
    {GPIOB, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_11},
    {GPIOD, GPIO_PIN_5},
    {GPIOD, GPIO_PIN_4},
    {GPIOD, GPIO_PIN_3},
    {GPIOD, GPIO_PIN_2},
    {GPIOD, GPIO_PIN_1},
    {GPIOD, GPIO_PIN_0},
    {GPIOA, GPIO_PIN_10},
    {GPIOD, GPIO_PIN_9},
    {GPIOD, GPIO_PIN_8},
    {GPIOC, GPIO_PIN_7}, // ...
    {GPIOC, GPIO_PIN_6}, // U47
    {GPIOA, GPIO_PIN_9}, // U48
    {GPIOA, GPIO_PIN_11}, // up
    {GPIOA, GPIO_PIN_15}, // down
    {GPIOC, GPIO_PIN_9}, // left
    {GPIOA, GPIO_PIN_12}, // right
    {GPIOC, GPIO_PIN_8} // center
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"kbd0\"}\n", sizeof("{\"name\":\"kbd0\"}\n") - 1}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    static bool states[BTN_CNT];
    memset(states, true, BTN_CNT);
    while(1) {
        static uint8_t vals[BTN_CNT];
        uint8_t val_count = 0;
        for(uint8_t i = 0; i < BTN_CNT; i++) {
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

    LOGLN("start kbd0");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);

    static mcp_module_stm32_mcp_fs_t mmfs;
    static uint8_t mmfs_aligned_aux_memory[MCP_MODULE_STM32_MCP_FS_AUX_MEMORY_SIZE(APP_FS_BLOCK_COUNT)] __attribute__((aligned));

    res = mcp_module_stm32_mcp_fs_init(&mmfs, mmfs_aligned_aux_memory, ((uintptr_t)fs_bin - FLASH_BASE) / FLASH_PAGE_SIZE, APP_FS_BLOCK_COUNT);
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
