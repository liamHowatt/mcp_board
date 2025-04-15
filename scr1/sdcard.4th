mcpd_driver_connect
0= assert
constant con

here 1 allot constant buf

con buf 1 mcpd_read
buf c@ constant socketno

con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_SDCARD mcpd_resource_acquire
dup 0 >= assert constant resource_id

: route ( io_type pinno -- )
	2>r con resource_id 2r> socketno swap
	mcpd_resource_route 0= assert
;

MCP_PINS_PIN_SPI_CLK  3 route
MCP_PINS_PIN_SPI_MOSI 1 route
MCP_PINS_PIN_SPI_CS   0 route
MCP_PINS_PIN_SPI_MISO 1 route

0xff buf c! \ end protocol
con buf 1 mcpd_write

begin
	10000 ms
again
