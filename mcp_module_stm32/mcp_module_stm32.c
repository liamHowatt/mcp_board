#include "mcp_module_stm32.h"

static bool bb_read_cb(void * caller_ctx, mbb_cli_pin_t pinno)
{
    const mcp_module_stm32_pin_t * clk_dat_pins = caller_ctx;
    const mcp_module_stm32_pin_t * pin = clk_dat_pins + pinno;
    return HAL_GPIO_ReadPin(pin->port, pin->pin);
}

static void bb_write_cb(void * caller_ctx, mbb_cli_pin_t pinno, bool val)
{
    const mcp_module_stm32_pin_t * clk_dat_pins = caller_ctx;
    const mcp_module_stm32_pin_t * pin = clk_dat_pins + pinno;
    HAL_GPIO_WritePin(pin->port, pin->pin, val);
}

static void delay_us_cb(void * user_ctx, uint32_t us)
{
    if(us <= 2000) {
        HAL_Delay(2);
    } else {
        HAL_Delay(us / 1000);
    }
}

static void wait_clk_high_cb(void * user_ctx)
{
    while(!bb_read_cb(user_ctx, MBB_CLI_PIN_CLK));
}

void mcp_module_stm32_run(
    const mcp_module_stm32_pin_t * clk_dat_pins,
    const mcp_module_static_file_table_entry_t * static_file_table,
    uint32_t static_file_table_size,
    void * rw_fs_ctx,
    const mcp_module_rw_fs_vtable_t * rw_fs_vtable
)
{
    mcp_module_run(
        (void *) clk_dat_pins,
        bb_read_cb,
        bb_write_cb,
        delay_us_cb,
        wait_clk_high_cb,
        static_file_table,
        static_file_table_size,
        rw_fs_ctx,
        rw_fs_vtable
    );
}
