#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_5}
};

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"WELL_DONE", NULL, 0},
    {"info", "one\ntwo\nthree\n", 14}
};

void app_main(void) {
    LOGLN("start scr1");

    for(int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);
        HAL_Delay(200);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
        HAL_Delay(200);
    }

    mcp_module_stm32_run(
        clk_dat_pins,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        NULL,
        NULL
    );
}
