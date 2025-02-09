#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

void app_main(void) {
    unsigned x = 0;
    while(1) {
        LOGLN("%u", x++);
        HAL_Delay(100);
    }
}
