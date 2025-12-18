\ set to 0 or 1
1 constant use_compression
1 constant use_uart

\ max 48 MHz
10000000 constant normfreq

22 constant buf_size
here buf_size allot constant buf

mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

use_uart buf c!
con buf 1 mcpd_write

con buf 1 mcpd_read
buf c@ constant socketno

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
: getfreq seq spi_sequence_s.frequency + @ ;

: settxbuf  trans spi_trans_s.txbuffer + ! ;
: gettxbuf  trans spi_trans_s.txbuffer + @ ;
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

: set_brightness ( val0-31 -- )
	gettxbuf getfreq 2>r
	buf settxbuf
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

	5 0 do
		over
		1 4 i - lshift
		and
		if 0x3f else 0x03 then
		q
	loop

	0x01 qdowrite

	2r> setfreq settxbuf

	drop
;

16000000 setfreq

150 ms \ just in case
buf LCDPI_RESET_or_LCDPI_RD q 0 q 0 qdowrite
50 ms
buf LCDPI_RD q 0 q 0 qdowrite
50 ms

32000000 setfreq
read1 dup 170 <> if ." warning: display product code was not 170. it was " dup . cr then drop
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
variable backlight_val
31 dup backlight_val ! set_brightness

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
	0xaaab w, 0x80ff w, 0x8cff w, 0x8ccf w, 0x98c9 w, 0xaabf w, 0xaaaf w, \ 0xaaab w,
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
		swap 1+ 1+ dup rot 2* codes + @ store
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
21 constant flush_queue_elem_size
align here constant flush_queue_buf flush_queue_elem_size allot
flush_queue flush_queue_buf flush_queue_elem_size 1 queue-init

: flush_thread ( arg -- ret )
	drop

	align here flush_queue_elem_size allot

	begin
		flush_queue over queue-get

		SSD1963_SET_PAGE_ADDRESS SSD1963_SET_COLUMN_ADDRESS 2 pick 20 + c@ 1 and if swap then 2>r

		r> 0 writecd
		dup x1 writecd1_uppper_lower
		dup x2 writecd1_uppper_lower
		r> 0 writecd
		dup y1 writecd1_uppper_lower
		dup y2 writecd1_uppper_lower

		SSD1963_WRITE_MEMORY_START 0 writecd

		dup 16 + @

		use_compression if
			1-
			start_byte over c!
			dup settxbuf
			1- dup
			2 pick lv_area_get_size
			lcdpi_compress
			1- setnwords
			trnioctl
			buf settxbuf
		else
			1- start_byte over c! settxbuf
			dup lv_area_get_size
			2* 1+ setnwords
			trnioctl
			buf settxbuf
		then

		flush_queue queue-task-done
	again

	drop flush_queue_elem_size negate allot

	0
;

: flush_cb ( disp area px_map -- )
	align here >r swap here 16 dup allot move , lv_display_get_rotation c,
	flush_queue r> queue-put
	flush_queue_elem_size negate allot
;

: flush_wait_cb ( disp -- )
	drop
	flush_queue queue-join
;

: delete_cb ( e -- )
	drop
	full_fb mcp_lvgl_static_fb_release
	fd close 0= assert
	\ TODO close ufd, remove poll, and join thread
	con mcpd_disconnect
;

: res_changed_cb ( e -- )
	lv_event_get_target
	lv_display_get_rotation
	dup 0= if
		0
	else dup 1 = if
		0x21
	else dup 2 = if
		0x03
	else dup 3 = if
		0x22
	else
		s" invalid display rotation" panic_msg
	then then then then
	nip

	0x36 0wcd1 \ rotation: SSD1963_SET_ADDRESS_MODE
	1wcd1
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
disp c' res_changed_cb LV_EVENT_RESOLUTION_CHANGED 0 lv_display_add_event_cb
disp 3 lv_display_set_rotation

use_uart if
	con MCP_PINS_PERIPH_TYPE_UART MCP_PINS_DRIVER_TYPE_UART_RAW mcpd_resource_acquire
	dup 0 >= s" uart acquire fail" assert_msg
	constant uart_resource_id

	con uart_resource_id MCP_PINS_PIN_UART_RX socketno 0 mcpd_resource_route
	0= s" RX route fail" assert_msg

	con uart_resource_id mcpd_resource_get_path
	dup s" get UART path fail" assert_msg
	O_RDONLY 0 open
	dup 0 >= s" open UART fail" assert_msg
	constant ufd

	ufd 115200 mcpd_uart_set_baud
	0= s" baud set fail" assert_msg
then

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

: read_common ( -- )
	buf 2 + c@
	8 lshift
	buf 1+ c@ or
	8 lshift
	buf c@ or

	?dup if
		dup 0x3ff and swap
		10 rshift 0x3ff and

		\ 2dup ." x: " . ." y: " . cr
		70 960 0 799 lv_map touch_x !
		85 915 479 0 lv_map touch_y !
		LV_INDEV_STATE_PRESSED touch_state !
	else
		\ ." pen up" cr
		LV_INDEV_STATE_RELEASED touch_state !
	then

	indev lv_indev_read
;

: readall ( fd buf cnt -- )
	begin ?dup while
		2 pick 2 pick 2 pick read dup 0> s" readall read was not > 0" assert_msg
		dup >r
		-
		swap r> + swap
	repeat
	2drop
;
: read_uart_cb ( handle fd revents user_data -- )
	drop
	EPOLLIN = s" uart epoll revent was not EPOLLIN" assert_msg
	2drop

	ufd buf 3 readall

	begin
		read_common

		ufd 0 mcpd_uart_set_blocking 0= s" uart set non-blocking fail" assert_msg
		ufd buf 3 read
		ufd 1 mcpd_uart_set_blocking 0= s" uart set blocking fail" assert_msg

		dup 0< if
			drop
			exit
		then

		>r
		ufd buf r@ + 3 r> - readall
	again
;

: read_mcp_cb ( user_data -- )
	drop

	read_common

	con buf 3 c' read_mcp_cb 0 mcp_lvgl_async_mcpd_read
;

indev LV_INDEV_TYPE_POINTER lv_indev_set_type
indev c' indev_cb lv_indev_set_read_cb
indev LV_INDEV_MODE_EVENT lv_indev_set_mode
indev disp lv_indev_set_display

here constant LV_SYMBOL_GPS 0xEF c, 0x84 c, 0xA4 c, 0 c,

lv_screen_active lv_image_create
dup LV_SYMBOL_GPS lv_image_set_src
indev swap lv_indev_set_cursor

use_uart if
	ufd c' read_uart_cb EPOLLIN 0 mcp_lvgl_poll_add drop
else
	con buf 3 c' read_mcp_cb 0 mcp_lvgl_async_mcpd_read
then

: slider_event_cb ( e -- )
	lv_event_get_target_obj
	lv_slider_get_value
	dup backlight_val !
	set_brightness
;

: rotate_btn_cb ( e -- )
	drop
	disp lv_display_get_rotation
	1+ 3 and
	disp swap lv_display_set_rotation
;

: pointer_checkbox_cb ( e -- )
	indev lv_indev_get_cursor
	LV_OBJ_FLAG_HIDDEN
	rot
	lv_event_get_target_obj
	LV_STATE_CHECKED lv_obj_has_state
	1 xor lv_obj_set_flag
;

: ok_btn_cb ( e -- )
	lv_event_get_target_obj
	lv_obj_get_parent
	lv_obj_delete
;

: settings_app_cb ( base_obj -- )
	dup LV_FLEX_FLOW_COLUMN lv_obj_set_flex_flow
	dup 40 0 lv_obj_set_style_pad_all
	dup 20 0 lv_obj_set_style_pad_row

	dup lv_slider_create
	dup 100 lv_pct 0 lv_obj_set_style_max_width
	dup 1 31 lv_slider_set_range
	dup backlight_val @ 0 lv_slider_set_value
	c' slider_event_cb LV_EVENT_VALUE_CHANGED 0 lv_obj_add_event_cb drop

	dup lv_button_create
	dup c' rotate_btn_cb LV_EVENT_CLICKED 0 lv_obj_add_event_cb drop
	lv_label_create
	s" rotate" drop lv_label_set_text_static

	dup lv_checkbox_create
	dup s" show touch pointer" drop lv_checkbox_set_text_static
	dup LV_STATE_CHECKED indev lv_indev_get_cursor LV_OBJ_FLAG_HIDDEN lv_obj_has_flag 1 xor lv_obj_set_state
	c' pointer_checkbox_cb LV_EVENT_VALUE_CHANGED 0 lv_obj_add_event_cb drop

	lv_button_create
	dup c' ok_btn_cb LV_EVENT_CLICKED 0 lv_obj_add_event_cb drop
	lv_label_create
	s" ok" drop lv_label_set_text_static
;

s" display settings" drop c' settings_app_cb mcp_lvgl_app_register

depth 0= s" oops, something is left on the stack" assert_msg
