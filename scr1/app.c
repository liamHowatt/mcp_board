#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

// #include "../mcp_bitbang/mcp_bitbang_client.h"

// #include <stddef.h>
// #include <inttypes.h>
#include <assert.h>

void app_main(void) {
    LOGLN("start scr1");

    // HAL_Delay(100); // give the base time to start up

    while(1) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);
        HAL_Delay(200);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
        HAL_Delay(200);
    }
}
