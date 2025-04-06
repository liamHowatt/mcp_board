mcpd_driver_connect
0= assert
constant con

here 1 allot
con over 1 mcpd_read
c@ constant socketno
-1 allot

con MCP_PINS_TYPE_SPI mcpd_resource_acquire
dup 0 >= assert constant resource_id

: route ( io_type pinno -- )
	2>r con resource_id 2r> socketno swap
	mcpd_resource_route 0= assert
;

MCP_PINS_SPI_CLK  3 route
MCP_PINS_SPI_MOSI 1 route
MCP_PINS_SPI_CS   2 route

con resource_id mcpd_resource_get_path
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

0xff buf c!
con buf 1 mcpd_write
con mcpd_disconnect

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

240 320 lv_display_create
dup fb 0 buf_sz LV_DISPLAY_RENDER_MODE_DIRECT lv_display_set_buffers
c' flush_cb lv_display_set_flush_cb

\ fd close -1 <> assert
