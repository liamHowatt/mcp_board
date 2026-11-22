#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"
#include "../mcp_module_stm32/mcp_module_stm32_mcp_fs.h"

#include "../mcp_assert.h"

#if FLASH_PAGE_SIZE != 2048
#error flash pages are not the expected size
#endif

#define APP_FS_BLOCK_COUNT 5

extern const unsigned char fs_bin[];

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_5}
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"batt0\"}\n", sizeof("{\"name\":\"batt0\"}\n") - 1}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    while(1) {
        uint8_t cmd;
        mcp_module_driver_read(hdl, &cmd, 1);
        assert(cmd == 0);

        HAL_StatusTypeDef status = HAL_ADC_Start(&hadc1);
        assert(status == HAL_OK);
        status = HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
        assert(status == HAL_OK);
        uint16_t adc_val = HAL_ADC_GetValue(&hadc1);
        status = HAL_ADC_Stop(&hadc1);
        assert(status == HAL_OK);

        mcp_module_driver_write(hdl, &adc_val, 2);
    }
}

void app_main(void) {
    int res;

    LOGLN("start batt0");

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
