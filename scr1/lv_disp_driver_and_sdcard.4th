mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

here 1 allot
con over 1 mcpd_read
c@ constant socketno
-1 allot

: route ( resource_id io_type pinno -- )
	>r 2>r con 2r> socketno r>
	\  con  resource_id  io_type  socketno  pinno
	mcpd_resource_route 0= s" mcpd_resource_route failure" assert_msg
;

\ disp
con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_RAW mcpd_resource_acquire
dup 0 >= s" mcpd_resource_acquire failure" assert_msg constant resource_id_disp

\ SD Card
con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_SDCARD mcpd_resource_acquire
dup 0 >= s" mcpd_resource_acquire failure" assert_msg constant resource_id_sd

\ disp
resource_id_disp MCP_PINS_PIN_SPI_CLK  3 route
resource_id_disp MCP_PINS_PIN_SPI_MOSI 1 route
resource_id_disp MCP_PINS_PIN_SPI_CS   2 route

\ SD Card
resource_id_sd   MCP_PINS_PIN_SPI_CS   0 route
resource_id_sd   MCP_PINS_PIN_SPI_MISO 1 route

con resource_id_disp mcpd_resource_get_path
dup s" get_path failure" assert_msg
O_RDONLY 0 open
dup -1 <> s" open failure" assert_msg constant fd

: callot ( size -- ptr )
	align here over allot
	dup rot 0 fill
;

spi_trans_s callot constant trans
spi_sequence_s callot constant seq

8       seq spi_sequence_s.nbits + c!
1       seq spi_sequence_s.ntrans + c!
trans   seq spi_sequence_s.trans + !

here 1 allot constant buf

: set_spi_for_cmds
	1000000 seq spi_sequence_s.frequency + !
	buf trans spi_trans_s.txbuffer + !
	1   trans spi_trans_s.nwords + !
;

set_spi_for_cmds

: s ( byte -- )
	buf c!
	fd SPIIOC_TRANSFER seq ioctl
	-1 <> s" byte transfer ioctl failure" assert_msg
;

: p ( pin val -- )
	swap 1 lshift or
	buf c!
	con buf 1 mcpd_write
	con buf 1 mcpd_read
;

0 constant BKLT
1 constant DC
2 constant RST

1 constant D
0 constant C

0x36 constant MADCTL
0x20 constant MADCTL_MV
0x40 constant MADCTL_MX
0x80 constant MADCTL_MY

0x2c constant MEMORY_WRITE

: send_short ( short -- )
	dup 8 rshift s
	s
;
: addr_set ( xstart xend op -- )
	DC C p
	s
	DC D p
	swap
	send_short
	send_short
;
: col_addr_set 0x2a addr_set ;
: row_addr_set 0x2b addr_set ;

BKLT 1 p

RST 0 p
500 ms
RST 1 p
500 ms

DC C p
0x01 s \ software reset
150 ms

\ DC C p
0x11 s \ sleep out
500 ms

\ DC C p
0x3a s \ pixel format
DC D p
0x55 s
10 ms

DC C p
0x21 s \ display inversion on
10 ms

\ DC C p
0x13 s \ partial off
10 ms

\ DC C p
MADCTL s
DC D p
MADCTL_MY MADCTL_MX or s

DC C p
0x29 s \ display on
500 ms

\ DC C p
MEMORY_WRITE s

DC D p

\ 0xff buf c! \ end protocol
\ con buf 1 mcpd_write

240 320 * 2 * constant buf_sz
buf_sz mcp_lvgl_static_fb_acquire dup s" fb acquire failure" assert_msg constant fb

: set_spi_for_fb
	48000000 seq spi_sequence_s.frequency + !
	fb       trans spi_trans_s.txbuffer + !
	buf_sz   trans spi_trans_s.nwords + !
;

set_spi_for_fb

: flush_cb ( disp area px_map -- )
	2drop
	dup lv_display_flush_is_last if
		fb buf_sz 2/
		2dup lv_draw_sw_rgb565_swap
		fd SPIIOC_TRANSFER seq ioctl
		-1 <> assert
		lv_draw_sw_rgb565_swap
	then
	lv_display_flush_ready
;

: delete_cb ( e -- )
	drop
	fb mcp_lvgl_static_fb_release
	fd close 0= s" close failure" assert_msg
	con mcpd_disconnect
;

: res_changed_cb ( e -- )
	lv_event_get_target
	lv_display_get_rotation
	dup 0= if
		MADCTL_MX MADCTL_MY or
	else dup 1 = if
		MADCTL_MY MADCTL_MV or
	else dup 2 = if
		0
	else dup 3 = if
		MADCTL_MX MADCTL_MV or
	else
		s" invalid display rotation" panic_msg
	then then then then

	set_spi_for_cmds
	DC C p
	MADCTL s
	DC D p
	s
	1 and 0= if
		0 240 col_addr_set
		0 320 row_addr_set
	else
		0 320 col_addr_set
		0 240 row_addr_set
	then
	DC C p
	MEMORY_WRITE s
	DC D p
	set_spi_for_fb
;

240 320 lv_display_create
dup LV_COLOR_FORMAT_RGB565 lv_display_set_color_format
dup fb 0 buf_sz LV_DISPLAY_RENDER_MODE_DIRECT lv_display_set_buffers
dup c' flush_cb lv_display_set_flush_cb
dup c' delete_cb LV_EVENT_DELETE 0 lv_display_add_event_cb
dup c' res_changed_cb LV_EVENT_RESOLUTION_CHANGED 0 lv_display_add_event_cb
constant disp

: rotate_cb ( base_obj -- )
	lv_obj_delete
	disp lv_display_get_rotation
	1+ 3 and
	disp swap lv_display_set_rotation
;

s" rotate display" drop c' rotate_cb mcp_lvgl_app_register