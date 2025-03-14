#pragma once
#include "hal_header.h"
#include <stdio.h>

void app_main(void);

// #define DBG_UART huart1
#ifdef DBG_UART
extern UART_HandleTypeDef DBG_UART;
extern char logln_buf[256];
#define LOGLN(format, ...) \
    HAL_UART_Transmit(&DBG_UART, (const uint8_t *) logln_buf, sprintf(logln_buf, format "\n", ## __VA_ARGS__), HAL_MAX_DELAY)
#define LOG(format, ...) \
    HAL_UART_Transmit(&DBG_UART, (const uint8_t *) logln_buf, sprintf(logln_buf, format, ## __VA_ARGS__), HAL_MAX_DELAY)
#else
#define LOGLN(format, ...)
#endif
