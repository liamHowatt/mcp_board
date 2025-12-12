\ set to 0 or 1
1 constant use_compression

mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

here 1 allot
con over 1 mcpd_read
c@ constant socketno
-1 allot

con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_RAW mcpd_resource_acquire
dup 0 >= s" SPI resource acquire failure" assert_msg constant resource_id

: route ( io_type pinno -- )
	2>r con resource_id 2r> socketno swap
	mcpd_resource_route 0= s" route failure" assert_msg
;

MCP_PINS_PIN_SPI_CLK  3 route
MCP_PINS_PIN_SPI_MOSI 1 route
MCP_PINS_PIN_SPI_CS   2 route
MCP_PINS_PIN_SPI_MISO 3 route

con resource_id mcpd_resource_get_path
dup s" get path failure" assert_msg
O_RDONLY 0 open
dup -1 <> s" open failure" assert_msg constant fd

: callot ( size -- ptr )
	align here over allot
	dup rot 0 fill
;

spi_trans_s callot constant trans
spi_sequence_s callot constant seq

8        seq spi_sequence_s.nbits + c!
1        seq spi_sequence_s.ntrans + c!
trans    seq spi_sequence_s.trans + !
: setfreq seq spi_sequence_s.frequency + ! ;
30000000 constant normfreq

22 constant buf_size
here buf_size allot constant buf

: settxbuf  trans spi_trans_s.txbuffer + ! ;
: setrxbuf  trans spi_trans_s.rxbuffer + ! ;
: setnwords trans spi_trans_s.nwords + ! ;
buf settxbuf

1 7 lshift constant LCDPI_LONG
1 6 lshift constant LCDPI_BLOCK
1 5 lshift constant LCDPI_RESET
1 4 lshift constant LCDPI_RS
1 3 lshift constant LCDPI_BL
1 2 lshift constant LCDPI_RD
LCDPI_RESET LCDPI_RD or constant LCDPI_RESET_or_LCDPI_RD

0x2a constant SSD1963_SET_COLUMN_ADDRESS
0x2b constant SSD1963_SET_PAGE_ADDRESS
0x2c constant SSD1963_WRITE_MEMORY_START

: q ( bufp byte -- bufp-plus1 )
	over buf - buf_size >= if s" buf is full" panic_msg then
	over c!
	1+
;

: qn ( bufp byte cnt -- bufp-plusn )
	0 do tuck q swap loop
	drop
;

: trnioctl ( -- )
	fd SPIIOC_TRANSFER seq ioctl
	-1 <> s" SPI transfer ioctl failure" assert_msg
;

: dowrite ( buf-end -- )
	buf - setnwords
	trnioctl
;

: qdowrite ( bufp byte -- ) q dowrite ;

: read1 ( -- b )
	here 1 allot
	0   settxbuf
	dup setrxbuf
	1 setnwords
	trnioctl
	c@
	-1 allot
	buf settxbuf
	0   setrxbuf
;

: writecd ( value isdata -- )
	over dup 0xff and <> if s" writecd value was not a byte" panic_msg then
	buf LCDPI_RESET_or_LCDPI_RD rot if LCDPI_RS or then q
	0 q
	swap qdowrite
;

: writecd1ms ( value isdata -- )
	writecd
	1 ms
;

: 0wcd1 ( value -- ) 0 writecd1ms ;
: 1wcd1 ( value -- ) 1 writecd1ms ;

16000000 setfreq

150 ms \ just in case
buf LCDPI_RESET_or_LCDPI_RD q 0 q 0 qdowrite
50 ms
buf LCDPI_RD q 0 q 0 qdowrite
50 ms

32000000 setfreq
." product code: " read1 . cr
normfreq setfreq

\ init onewire
150 ms \ just in case
400000 setfreq
buf 0xe9 q 0 q 0 qdowrite
5 ms
buf 0xe9 q 0 q 0 q
	0xff 6 qn
	0 12 qn
	1 qdowrite
1 ms
normfreq setfreq

150 ms \ just in case
buf LCDPI_RESET_or_LCDPI_RD q 0 q 0 qdowrite
50 ms
buf LCDPI_RD q 0 q 0 qdowrite
50 ms
buf LCDPI_RESET_or_LCDPI_RD q 0 q 0 qdowrite
150 ms

0xE2 0wcd1 \ SSD1963_SET_PLL_MN
0x23 1wcd1
0x02 1wcd1
0x04 1wcd1

0xE0 0wcd1 \ SSD1963_SET_PLL
0x01 1wcd1

1 ms

0xE0 0wcd1 \ SSD1963_SET_PLL
0x03 1wcd1

0x01 0wcd1 \ SSD1963_SOFT_RESET

5 ms

0xE6 0wcd1 \ SSD1963_SET_LSHIFT_FREQ
0x03 1wcd1
0x93 1wcd1
0x89 1wcd1

0xB0 0wcd1 \ SSD1963_SET_LCD_MODE
0x20 1wcd1
0x20 1wcd1
0x03 1wcd1
0x1F 1wcd1
0x01 1wcd1
0xDF 1wcd1
0x00 1wcd1

0xB4 0wcd1 \ SSD1963_SET_HORZ_PERIOD / SSD1963_SET_HORI_PERIOD
0x04 1wcd1
0x1F 1wcd1
0x00 1wcd1
0xD8 1wcd1
0x7F 1wcd1
0x00 1wcd1
0x00 1wcd1
0x00 1wcd1

0xB6 0wcd1 \ SSD1963_SET_VERT_PERIOD
0x01 1wcd1
0xFB 1wcd1
0x00 1wcd1
0x1B 1wcd1
0x03 1wcd1
0x00 1wcd1
0x00 1wcd1

0xBA 0wcd1 \ SSD1963_SET_GPIO_VALUE
0x0f 1wcd1

0xB8 0wcd1 \ SSD1963_SET_GPIO_CONFIG / SSD1963_SET_GPIO_CONF
0x0f 1wcd1
0x01 1wcd1

0xF0 0wcd1 \ SSD1963_SET_PIXEL_DATA_INTERFACE
0x03 1wcd1

10 ms

0x29 0wcd1 \ SSD1963_SET_DISPLAY_ON

45 ms \ prevent flash as display starts up. visible at 40ms

0x2A 0wcd1 \ SSD1963_SET_COLUMN_ADDRESS
0x00 1wcd1
0x00 1wcd1
0x01 1wcd1
0xfd 1wcd1

0x2B 0wcd1 \ SSD1963_SET_PAGE_ADDRESS
0x00 1wcd1
0x00 1wcd1
0x01 1wcd1
0x0f 1wcd1

0x36 0wcd1 \ rotation: SSD1963_SET_ADDRESS_MODE
\ LANDSCAPE_R
0x00 1wcd1 \ 0x0000, 0xC8

0xBC 0wcd1 \ SSD1963_SET_POST_PROC
0x40 1wcd1
0x80 1wcd1
0x40 1wcd1
0x01 1wcd1

\ tps61165 set brightness
150 ms \ just in case
500000 setfreq
buf
0xe9 q
0xff q
0xff q
0x03 q
0x3f q
0x3f q
0x3f q
0x03 q
0x03 q
0x3f q
0x03 q
0x00 q
0xff q
0x03 q
0x03 q
0x03 q
0x3f q
0x3f q
0x3f q
0x3f q
0x3f q
0x01 qdowrite
normfreq setfreq


: pre_mask_value ( hw -- hw )
	0xffdf and
;

: compute_value ( hw -- hw )
	dup 0xffc0 and 2/
	swap 0x001f and
	or
;

: pre_inc2 ( addr -- addr+2 addr+2 )
	1+ 1+ dup
;

: store ( dst val -- )
	2dup swap 1+ c!
	8 rshift swap c!
;

: xchg-dp ( new-dp -- old-dp )
	here tuck - allot
;

align here constant codes
	0xaaab , 0x80ff , 0x8cff , 0x8ccf , 0x98c9 , 0xaabf , 0xaaaf , \ 0xaaab ,
variable last_value
variable start
variable repeat_start

1 , -4 allot here c@ if \ detect endian
	\ little endian
	0xabaaabaa
	0xabaa
else	
	\ big endian
	0xaaabaaab
	0xaaab
then
constant code7
constant code7code7

: send_repeat_codes ( src dst -- src dst )

	over repeat_start @ - 2/ 1-

	?dup 0= if exit then

	dup 448 > if \ 7 * 64
		over 2 and 0= if
			7 - swap
			1+ 1+ code7 over w!
			swap
		then

		swap 1+ 1+ xchg-dp swap
		\ src old-dp count
		begin
			448 -
			code7code7
			dup , dup , dup , dup , dup , dup , dup , dup ,
			dup , dup , dup , dup , dup , dup , dup , dup ,
			dup , dup , dup , dup , dup , dup , dup , dup ,
			dup , dup , dup , dup , dup , dup , dup ,     ,
		dup 448 < until
		swap xchg-dp 1- 1- swap
	then

	begin dup 7 >= while
		7 -
		swap
		1+ 1+ code7 over w!
		swap
	repeat

	?dup if
		swap 1+ 1+ dup rot cells codes + @ store
	then
;

: lcdpi_compress ( src dst src_len -- out_len )
	>r
	dup start !
	swap pre_inc2 w@ last_value ! swap
	pre_inc2 last_value @ pre_mask_value compute_value store
	swap dup repeat_start !
	\ dst src
	r> 1 do
		1+ 1+ dup w@ last_value @ - if
			dup w@ last_value !
			swap
			\ src dst

			send_repeat_codes

			1+ 1+ dup last_value @ pre_mask_value compute_value store

			swap dup repeat_start !
			\ dst src
		then
	loop
	-2 repeat_start +!
	swap
	\ src dst

	send_repeat_codes

	nip
	start @ -
;


mcp_lvgl_static_fb_size constant full_fb_sz
0 mcp_lvgl_static_fb_acquire dup s" fb acquire failed" assert_msg constant full_fb
LCDPI_RS LCDPI_RESET or LCDPI_RD or LCDPI_LONG or use_compression if LCDPI_BLOCK or then constant start_byte

: x1 ( area -- x1 ) @ ;
: y1 ( area -- y1 ) 4 + @ ;
: x2 ( area -- x2 ) 8 + @ ;
: y2 ( area -- y2 ) 12 + @ ;

: writecd1_uppper_lower ( 16bitval -- )
	dup 8 rshift 1 writecd
	    0xff and 1 writecd
;

align here constant flush_queue queue-memsz allot

: flush_thread ( arg -- ret )
	drop

	align here 20 allot

	begin
		flush_queue over queue-get
		dup dup 16 + @

		SSD1963_SET_COLUMN_ADDRESS 0 writecd
		over x1 writecd1_uppper_lower
		over x2 writecd1_uppper_lower
		SSD1963_SET_PAGE_ADDRESS 0 writecd
		over y1 writecd1_uppper_lower
		over y2 writecd1_uppper_lower

		SSD1963_WRITE_MEMORY_START 0 writecd

		use_compression if
			1-
			start_byte over c!
			dup settxbuf
			1- dup
			rot lv_area_get_size
			lcdpi_compress
			1- setnwords
			trnioctl
			buf settxbuf
		else
			1- start_byte over c! settxbuf
			lv_area_get_size
			2* 1+ setnwords
			trnioctl
			buf settxbuf
		then

		flush_queue queue-task-done
	again

	0
;

: flush_cb ( disp area px_map -- )
	align here >r swap here 16 dup allot move , drop
	flush_queue r> queue-put
	-20 allot
;

: flush_wait_cb ( disp -- )
	drop
	flush_queue queue-join
;

: delete_cb ( e -- )
	drop
	full_fb mcp_lvgl_static_fb_release
	fd close 0= assert
	con mcpd_disconnect
;

: align-4-up ( ptr len -- alptr new-len )
	swap \ len ptr
	dup  \ len ptr ptr
	3 + 3 invert and \ len ptr alptr
	dup >r
	- \ len len-delta
	+ \ new-len
	r> \ new-len alptr
	swap \ alptr new-len
;

align here constant flush_queue_buf 20 allot
flush_queue flush_queue_buf 20 1 queue-init
0 1 ' flush_thread thread-create constant thread_hdl

800 480 lv_display_create constant disp
disp use_compression if LV_COLOR_FORMAT_RGB565 else LV_COLOR_FORMAT_RGB565_SWAPPED then lv_display_set_color_format
disp
	full_fb                 1+ full_fb_sz 2/ 1- align-4-up >r
	full_fb full_fb_sz 2/ + 1+ full_fb_sz 2/ 1- align-4-up
	r> min
	LV_DISPLAY_RENDER_MODE_PARTIAL
	lv_display_set_buffers
disp c' flush_cb lv_display_set_flush_cb
disp c' delete_cb LV_EVENT_DELETE 0 lv_display_add_event_cb
disp c' flush_wait_cb lv_display_set_flush_wait_cb

variable touch_x
variable touch_y
variable touch_state

: indev_cb ( indev data -- )
	touch_x @ over lv_indev_data_t.point + !
	touch_y @ over lv_indev_data_t.point + cell+ !
	touch_state @ over lv_indev_data_t.state + !
	2drop
;

lv_indev_create constant indev

: read_cb ( user_data -- )
	drop

	buf 2 + c@
	8 lshift
	buf 1+ c@ or
	8 lshift
	buf c@ or

	?dup if
		dup 0x3ff and swap
		10 rshift 0x3ff and

		\ 2dup ." x: " . ." y: " . cr
		799 * 1023 / touch_x !
		1023 swap - 479 * 1023 / touch_y !
		LV_INDEV_STATE_PRESSED touch_state !
	else
		\ ." pen up" cr
		LV_INDEV_STATE_RELEASED touch_state !
	then

	indev lv_indev_read

	con buf 3 c' read_cb 0 mcp_lvgl_async_mcpd_read
;

indev LV_INDEV_TYPE_POINTER lv_indev_set_type
indev c' indev_cb lv_indev_set_read_cb
indev LV_INDEV_MODE_EVENT lv_indev_set_mode
indev disp lv_indev_set_display

here constant LV_SYMBOL_GPS 0xEF c, 0x84 c, 0xA4 c, 0 c,

lv_screen_active lv_image_create
dup LV_SYMBOL_GPS lv_image_set_src
indev swap lv_indev_set_cursor

con buf 3 c' read_cb 0 mcp_lvgl_async_mcpd_read

depth 0= s" oops, something is left on the stack" assert_msg
