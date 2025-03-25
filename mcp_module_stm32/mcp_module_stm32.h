#pragma once

#ifndef MCP_MODULE_STM32_HAL_HEADER
#error MCP_MODULE_STM32_HAL_HEADER must be defined
#endif

#include "../mcp_module/mcp_module.h"
#include MCP_MODULE_STM32_HAL_HEADER

typedef struct {GPIO_TypeDef *port; uint16_t pin;} mcp_module_stm32_pin_t;

typedef
    #ifdef __HAL_TIM_GET_COUNTER
        TIM_HandleTypeDef
    #else
        void
    #endif
microsecond_timer_t;

void mcp_module_stm32_run(
    const mcp_module_stm32_pin_t * clk_dat_pins,
    microsecond_timer_t * microsecond_timer,
    const mcp_module_static_file_table_entry_t * static_file_table,
    uint32_t static_file_table_size,
    void * rw_fs_ctx,
    const mcp_module_rw_fs_vtable_t * rw_fs_vtable,
    void * driver_protocol_ctx,
    mcp_module_driver_protocol_cb_t driver_protocol_cb
);
