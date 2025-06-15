mcpd_driver_connect
0= assert
constant con

here 1 allot
con over 1 mcpd_read
c@ constant socketno
-1 allot

: route ( resource_id io_type pinno -- )
	>r 2>r con 2r> socketno r>
	\  con  resource_id  io_type  socketno  pinno
	mcpd_resource_route 0= assert
;

\ disp
con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_RAW mcpd_resource_acquire
dup 0 >= assert constant resource_id_disp

\ SD Card
con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_SDCARD mcpd_resource_acquire
dup 0 >= assert constant resource_id_sd

\ disp
resource_id_disp MCP_PINS_PIN_SPI_CLK  3 route
resource_id_disp MCP_PINS_PIN_SPI_MOSI 1 route
resource_id_disp MCP_PINS_PIN_SPI_CS   2 route

\ SD Card
resource_id_sd   MCP_PINS_PIN_SPI_CS   0 route
resource_id_sd   MCP_PINS_PIN_SPI_MISO 1 route

con resource_id_disp mcpd_resource_get_path
dup assert
O_RDONLY 0 open
dup -1 <> assert constant fd

: callot ( size -- ptr )
	align here over allot
	dup rot 0 fill
;

spi_trans_s callot constant trans
spi_sequence_s callot constant seq

8       seq spi_sequence_s.nbits + c!
1000000 seq spi_sequence_s.frequency + !
1       seq spi_sequence_s.ntrans + c!
trans   seq spi_sequence_s.trans + !

here 1 allot constant buf

buf trans spi_trans_s.txbuffer + !
1   trans spi_trans_s.nwords + !

: s ( byte -- )
	buf c!
	fd SPIIOC_TRANSFER seq ioctl
	-1 <> assert
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

BKLT 1 p

RST 0 p
500 ms
RST 1 p
500 ms

DC C p
0x01 s
150 ms

\ DC C p
0x11 s
500 ms

\ DC C p
0x3a s
DC D p
0x55 s
10 ms

DC C p
0x36 s
DC D p
0x08 s

DC C p
0x21 s
10 ms

\ DC C p
0x13 s
10 ms

\ DC C p
0x36 s
DC D p
0xc0 s

DC C p
0x29 s
500 ms

\ DC C p
0x2c s

DC D p

0xff buf c! \ end protocol
con buf 1 mcpd_write

240 320 * 2 * constant buf_sz
buf_sz malloc dup 0<> assert constant fb

48000000 seq spi_sequence_s.frequency + !
fb       trans spi_trans_s.txbuffer + !
buf_sz   trans spi_trans_s.nwords + !

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
	fb free
	fd close 0= assert
	con mcpd_disconnect
;

240 320 lv_display_create
dup fb 0 buf_sz LV_DISPLAY_RENDER_MODE_DIRECT lv_display_set_buffers
dup c' flush_cb lv_display_set_flush_cb
c' delete_cb LV_EVENT_DELETE 0 lv_display_add_event_cb
