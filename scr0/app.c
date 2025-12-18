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

#define APP_FS_BLOCK_COUNT 4

#define APP_UART huart2
extern UART_HandleTypeDef huart2;

extern const unsigned char fs_bin[];

struct xyp {
    uint32_t x;
    uint32_t y;
    bool pen;
};

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_5}
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"scr0\"}\n", sizeof("{\"name\":\"scr0\"}\n") - 1}
};

static void read_xyp(struct xyp *xyp) {
    uint8_t buf[5];
    HAL_StatusTypeDef hal_res;
    do {
        HAL_Delay(20);
        hal_res = HAL_I2C_Master_Receive(
            &TOUCH_I2C,
            0x4d << 1,
            buf,
            5,
            HAL_MAX_DELAY
        );
    } while (hal_res != HAL_OK);
    xyp->x = ((buf[2] & 0x1f) << 7) | (buf[1] & 0x7f);
    xyp->y = ((buf[4] & 0x1f) << 7) | (buf[3] & 0x7f);
    xyp->pen = 0 != (buf[0] & 1);
}

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t use_uart;
    mcp_module_driver_read(hdl, &use_uart, 1);

    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);

    while(1) {
        struct xyp xyp;
        uint8_t buf[3];

        do {
            read_xyp(&xyp);
        } while(!xyp.pen);

        do {
            uint32_t packet = 1 << 10;
            packet |= xyp.x >> 2;
            packet <<= 10;
            packet |= xyp.y >> 2;

            buf[0] = packet & 0xff;
            packet >>= 8;
            buf[1] = packet & 0xff;
            packet >>= 8;
            buf[2] = packet;

            if(use_uart) HAL_UART_Transmit(&APP_UART, buf, 3, HAL_MAX_DELAY);
            else mcp_module_driver_write(hdl, buf, 3);

            read_xyp(&xyp);
        } while(xyp.pen);

        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        if(use_uart) HAL_UART_Transmit(&APP_UART, buf, 3, HAL_MAX_DELAY);
        else mcp_module_driver_write(hdl, buf, 3);
    }
}

void app_main(void) {
    int res;

    LOGLN("start scr0");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);

    static mcp_module_stm32_mcp_fs_heatshrink_t mmfs;
    static uint8_t mmfs_aligned_aux_memory[MCP_MODULE_STM32_MCP_FS_AUX_MEMORY_SIZE(APP_FS_BLOCK_COUNT)] __attribute__((aligned));

    res = mcp_module_stm32_mcp_fs_heatshrink_init(&mmfs, mmfs_aligned_aux_memory, ((uintptr_t)fs_bin - FLASH_BASE) / FLASH_PAGE_SIZE, APP_FS_BLOCK_COUNT);
    assert(res == 0);

    mcp_module_stm32_run(
        clk_dat_pins,
        &MICROSECOND_TIMER,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        &mmfs,
        &mcp_module_stm32_mcp_fs_heatshrink_rw_fs_vtable,
        NULL,
        driver_protocol_cb
    );
}
