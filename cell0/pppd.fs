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

: set_dp ( new_dp -- )
	here - allot
;

: send ( c-addr u -- )
	here >r
	0 do
		dup c@ c,
		1+
	loop
	drop
	13 c, \ \r
	10 c, \ \n
	fd r@ here r@ - write
	here r@ - = s" send write issue" assert_msg
	r> set_dp
;

: blk fd 1 mcpd_uart_set_blocking drop ;
: nblk fd 0 mcpd_uart_set_blocking drop ;
: maybe_get fd buf 1 read 1 = dup if buf c@ emit then ;
: mb 1000 ms nblk begin maybe_get 0= until blk ;

s" AT" send mb
s" AT" send mb
s" AT+CPIN?" send mb
s" AT+CEREG?" send mb
s" AT+CGATT?" send mb
." done getting" cr
fd close 0= s" close fd issue" assert_msg
3000 ms

: strlen
	dup 1-
	begin 1+ dup c@ 0= until
	swap -
;

align here constant pppd_settings pppd_settings_s allot
pppd_settings pppd_settings_s 0 fill
get_path dup strlen dup TTYNAMSIZ < s" ttyname field too small" assert_msg
pppd_settings pppd_settings_s.ttyname + swap move
s" ABORT \"NO CARRIER\" \
ABORT \"NO DIALTONE\" \
ABORT \"ERROR\" \
ABORT \"NO ANSWER\" \
ABORT \"BUSY\" \
TIMEOUT 120 \
\"\" AT \
OK ATE1 \
OK AT+CGDCONT=1,\\\"IP\\\",\\\"inet.bell.ca\\\" \
OK ATD*99# \
CONNECT \\c" drop
pppd_settings pppd_settings_s.connect_script + !

pppd_settings pppd drop
." pppd returned" cr

con mcpd_disconnect
