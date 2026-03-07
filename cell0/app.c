#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_8},
    {GPIOA, GPIO_PIN_12}
};

static const char main_4th_body[] = R"(
)";

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"cell0\"}\n", sizeof("{\"name\":\"cell0\"}\n") - 1},
    {"main.4th", main_4th_body, sizeof(main_4th_body) - 1}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);
}

void app_main(void) {
    LOGLN("start cell0");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);

    mcp_module_stm32_run(
        clk_dat_pins,
        &MICROSECOND_TIMER,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        NULL,
        NULL,
        NULL,
        driver_protocol_cb
    );
}
