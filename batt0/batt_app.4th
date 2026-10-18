mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

6600 constant multiplier \ VCC*2 = 6.6 V
4095 constant divisor \ 12-bit sample
2000 constant refresh_period
variable timer_cb_p \ forward-declaration
align here 2 allot constant rd_buf
here 0 c, constant wr_buf
here 0 c, constant ioing
variable label 0 label !
variable timer 0 timer !

: ok_btn_cb ( e -- )
	drop

	timer @ ?dup if
		lv_timer_delete
		0 timer !
	then

	label @ lv_obj_get_parent
	lv_obj_delete
	0 label !
;

: rd_cb ( user_data -- )
	drop
	0 ioing c!

	label @
	?dup 0= if
		exit
	then
	s" %u mV" drop
	rd_buf w@ multiplier * divisor /
	( label fmt mv_val )
	3 lv_label_set_text_fmt

	timer_cb_p @ refresh_period 0 lv_timer_create
	dup 1 lv_timer_set_repeat_count
	timer !
;

: wr_cb ( user_data -- )
	drop
	con rd_buf 2 c' rd_cb 0 mcp_lvgl_async_mcpd_read
;

: send_request ( -- )
	1 ioing c!
	con wr_buf 1 c' wr_cb 0 mcp_lvgl_async_mcpd_write
;

: timer_cb ( timer -- )
	drop
	0 timer !
	send_request
;
c' timer_cb timer_cb_p !

: batt_app_cb ( base_obj -- )
	dup LV_FLEX_FLOW_COLUMN lv_obj_set_flex_flow
	dup 40 0 lv_obj_set_style_pad_all
	dup 20 0 lv_obj_set_style_pad_row

	dup lv_label_create
	dup s" checking voltage..." drop lv_label_set_text_static
	label !

	lv_button_create
	dup c' ok_btn_cb LV_EVENT_CLICKED 0 lv_obj_add_event_cb drop
	lv_label_create
	s" ok" drop lv_label_set_text_static

	ioing c@ 0= if
		send_request
	then
;

s" battery" drop c' batt_app_cb mcp_lvgl_app_register
