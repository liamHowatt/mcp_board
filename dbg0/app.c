#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"

#include <stddef.h>
#include <inttypes.h>

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOA, GPIO_PIN_8},
    {GPIOA, GPIO_PIN_12}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);
}

void app_main(void) {
    LOGLN("start dbg0");

    mcp_module_stm32_run(
        clk_dat_pins,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        NULL,
        driver_protocol_cb
    );
}
