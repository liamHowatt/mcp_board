#include "mcp_module_stm32.h"
#include <assert.h>

typedef struct {
    const mcp_module_stm32_pin_t * clk_dat_pins;
    microsecond_timer_t * microsecond_timer;
} ctx_t;

static bool bb_read_cb(void * caller_ctx, mbb_cli_pin_t pinno)
{
    ctx_t * ctx = caller_ctx;
    const mcp_module_stm32_pin_t * pin = ctx->clk_dat_pins + pinno;
    return HAL_GPIO_ReadPin(pin->port, pin->pin);
}

static void bb_write_cb(void * caller_ctx, mbb_cli_pin_t pinno, bool val)
{
    ctx_t * ctx = caller_ctx;
    const mcp_module_stm32_pin_t * pin = ctx->clk_dat_pins + pinno;
    HAL_GPIO_WritePin(pin->port, pin->pin, val);
}

static void delay_us_cb(void * hal_ctx, uint32_t us)
{
    ctx_t * ctx = hal_ctx;
#ifdef __HAL_TIM_GET_COUNTER
    if(ctx->microsecond_timer && us < 0xffffu - 5000u) {
        uint16_t us_u16 = us;
        uint16_t start = __HAL_TIM_GET_COUNTER(ctx->microsecond_timer);
        uint16_t now;
        uint16_t diff;
        do {
            now = __HAL_TIM_GET_COUNTER(ctx->microsecond_timer);
            diff = now - start;
        } while(diff < us_u16);
    }
#else
    assert(!ctx->microsecond_timer);
    if(0);
#endif
    else {
        if(us <= 2000) {
            HAL_Delay(2);
        } else {
            HAL_Delay((us + 999) / 1000);
        }
    }
}

static void wait_clk_high_cb(void * hal_ctx)
{
    while(!bb_read_cb(hal_ctx, MBB_CLI_PIN_CLK));
}

void mcp_module_stm32_run(
    const mcp_module_stm32_pin_t * clk_dat_pins,
    microsecond_timer_t * microsecond_timer,
    const mcp_module_static_file_table_entry_t * static_file_table,
    uint32_t static_file_table_size,
    void * rw_fs_ctx,
    const mcp_module_rw_fs_vtable_t * rw_fs_vtable,
    void * driver_protocol_ctx,
    mcp_module_driver_protocol_cb_t driver_protocol_cb
)
{
    ctx_t ctx = {clk_dat_pins, microsecond_timer};

    mcp_module_run(
        &ctx,
        bb_read_cb,
        bb_write_cb,
        delay_us_cb,
        wait_clk_high_cb,
        static_file_table,
        static_file_table_size,
        rw_fs_ctx,
        rw_fs_vtable,
        driver_protocol_ctx,
        driver_protocol_cb
    );
}
