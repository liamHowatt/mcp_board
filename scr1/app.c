#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"
#include "../mcp_module_stm32/mcp_module_stm32_mcp_fs.h"

#include <assert.h>

#if FLASH_PAGE_SIZE != 2048
#error flash pages are not the expected size
#endif

#define APP_FS_BLOCK_COUNT 5

#define BACKLIGHT 0
#define CMDDATA   1
#define RESET     2

#define BACKLIGHT_PINS GPIOA, GPIO_PIN_1
#define CMDDATA_PINS   GPIOA, GPIO_PIN_2
#define RESET_PINS     GPIOA, GPIO_PIN_3

extern const unsigned char fs_bin[];

static volatile uint8_t g_backlight_val;
static volatile uint8_t g_backlight_cycle;

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_5}
};

static const mcp_module_stm32_pin_t ctrl_gpios[3] = {
    [BACKLIGHT] = {BACKLIGHT_PINS},
    [CMDDATA  ] = {CMDDATA_PINS  },
    [RESET    ] = {RESET_PINS    },
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"scr1\"}\n", sizeof("{\"name\":\"scr1\"}\n") - 1}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);

    while(1) {
        uint8_t cmd;
        mcp_module_driver_read(hdl, &cmd, 1);
        if(cmd == 0xff) break;

        uint8_t pin = cmd & 0x3;
        uint8_t val = cmd >> 2;

        if(pin == BACKLIGHT) {
            g_backlight_val = val;
        }
        else if(pin <= RESET) {
            const mcp_module_stm32_pin_t * chip_pin = &ctrl_gpios[pin];
            HAL_GPIO_WritePin(chip_pin->port, chip_pin->pin, val != 0);
        }

        mcp_module_driver_write(hdl, &cmd, 1);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    uint8_t cycle = g_backlight_cycle;
    cycle += 1;
    if(cycle >= 10) cycle = 0;
    g_backlight_cycle = cycle;

    HAL_GPIO_WritePin(BACKLIGHT_PINS, cycle < g_backlight_val);
}

void app_main(void) {
    int res;

    LOGLN("start scr1");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);
    HAL_TIM_Base_Start_IT(&SOFTPWM_TIMER);

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
