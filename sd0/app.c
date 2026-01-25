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
mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

here 1 allot
con over 1 mcpd_read
c@ constant socketno
-1 allot

con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_SDCARD mcpd_resource_acquire
dup 0 >= s" mcpd_resource_acquire failure" assert_msg constant resource_id

: route ( io_type pinno -- )
	>r >r con resource_id r> socketno r>
	\  con  resource_id  io_type  socketno  pinno
	mcpd_resource_route 0= s" mcpd_resource_route failure" assert_msg
;

MCP_PINS_PIN_SPI_CLK  2 route
MCP_PINS_PIN_SPI_MOSI 1 route
MCP_PINS_PIN_SPI_CS   0 route
MCP_PINS_PIN_SPI_MISO 3 route

con resource_id mcpd_resource_get_path
dup s" sd get_path failure" assert_msg
s" /mnt/sd" drop s" littlefs" drop 0 0 mount
if ." error mounting sd card as littlefs" cr then

\ need to keep the con open to retain the sd resource
begin 30000 ms again
)";

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"sd0\"}\n", sizeof("{\"name\":\"sd0\"}\n") - 1},
    {"main.4th", main_4th_body, sizeof(main_4th_body) - 1}
};

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);
}

void app_main(void) {
    LOGLN("start sd0");

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
