align here constant event
keyboard_event_s allot
event keyboard_event_s 0 fill

here constant table
103 c, 108 c, 105 c, 106 c, 1 c, 28 c,

mcpd_driver_connect
0= assert
constant con

here constant buf
1 allot

s" /dev/ukeyboard" drop O_WRONLY 0 open
dup 0 >= assert constant fd

begin
	con buf 1 mcpd_read
	buf c@
	dup 1 and
		event keyboard_event_s.type + !
	1 rshift dup 6 < assert dup 0 >= assert table + c@
		event keyboard_event_s.code + !
	fd event keyboard_event_s write keyboard_event_s = assert
again
