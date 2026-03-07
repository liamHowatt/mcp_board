here 1 allot constant buf

mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

con buf 1 mcpd_read
buf c@ constant socketno

con socketno 0 mcpd_gpio_acquire
dup 0 >= s" mcpd_gpio_acquire failure" assert_msg
constant gpio_id

con MCP_PINS_PERIPH_TYPE_UART MCP_PINS_DRIVER_TYPE_UART_RAW mcpd_resource_acquire
dup 0 >= s" uart acquire fail" assert_msg
constant resource_id

con resource_id MCP_PINS_PIN_UART_RX socketno 1 mcpd_resource_route
0= s" RX route fail" assert_msg

con resource_id MCP_PINS_PIN_UART_TX socketno 1 mcpd_resource_route
0= s" TX route fail" assert_msg

: get_path
        con resource_id mcpd_resource_get_path
        dup s" get UART path fail" assert_msg
;

get_path O_RDWR 0 open
dup 0 >= s" open UART fail" assert_msg
constant fd

fd 115200 mcpd_uart_set_baud
0= s" baud set fail" assert_msg

con gpio_id 1 mcpd_gpio_set
4000 ms
con gpio_id 0 mcpd_gpio_set
4000 ms

: read_thread ( arg -- ret )
        drop
        here 1 allot

        ." begin repl reader" cr
        begin
                fd over 1 read 1 = s" thread read fail" assert_msg
                dup c@ emit
        again

        0
;

0 0 ' read_thread thread-create drop


: repl_write ( c -- )
        buf c!
        fd buf 1 write 1 = s" repl write fail" assert_msg
;
." begin repl writer" cr
begin
        key
        dup 10 = if 13 repl_write then
        repl_write
again

con mcpd_disconnect
