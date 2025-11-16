: '\n' 0x0a ;
: sp 0x20 ;
: '!' 0x21 ;
: '"' 0x22 ;
: '#' 0x23 ;
: '$' 0x24 ;
: '%' 0x25 ;
: '&' 0x26 ;
: ''' 0x27 ;
: '(' 0x28 ;
: ')' 0x29 ;
: '*' 0x2a ;
: '+' 0x2b ;
: ',' 0x2c ;
: '-' 0x2d ;
: '.' 0x2e ;
: '/' 0x2f ;
: '0' 0x30 ;
: '1' 0x31 ;
: '2' 0x32 ;
: '3' 0x33 ;
: '4' 0x34 ;
: '5' 0x35 ;
: '6' 0x36 ;
: '7' 0x37 ;
: '8' 0x38 ;
: '9' 0x39 ;
: ':' 0x3a ;
: ';' 0x3b ;
: '<' 0x3c ;
: '=' 0x3d ;
: '>' 0x3e ;
: '?' 0x3f ;
: '@' 0x40 ;
: '[' 0x5b ;
: '\' 0x5c ;
: ']' 0x5d ;
: '^' 0x5e ;
: '_' 0x5f ;
: '`' 0x60 ;
: 'a' 0x61 ;
: 'b' 0x62 ;
: 'c' 0x63 ;
: 'd' 0x64 ;
: 'e' 0x65 ;
: 'f' 0x66 ;
: 'g' 0x67 ;
: 'h' 0x68 ;
: 'i' 0x69 ;
: 'j' 0x6a ;
: 'k' 0x6b ;
: 'l' 0x6c ;
: 'm' 0x6d ;
: 'n' 0x6e ;
: 'o' 0x6f ;
: 'p' 0x70 ;
: 'q' 0x71 ;
: 'r' 0x72 ;
: 's' 0x73 ;
: 't' 0x74 ;
: 'u' 0x75 ;
: 'v' 0x76 ;
: 'w' 0x77 ;
: 'x' 0x78 ;
: 'y' 0x79 ;
: 'z' 0x7a ;
: '{' 0x7b ;
: '|' 0x7c ;
: '}' 0x7d ;
: '~' 0x7e ;

: sh 255 ;
: shpl 254 ;

LV_KEY_UP 253 <= s" key constants too big" assert_msg
LV_KEY_DOWN 253 <= s" key constants too big" assert_msg
LV_KEY_RIGHT 253 <= s" key constants too big" assert_msg
LV_KEY_LEFT 253 <= s" key constants too big" assert_msg
LV_KEY_ESC 253 <= s" key constants too big" assert_msg
LV_KEY_DEL 253 <= s" key constants too big" assert_msg
LV_KEY_BACKSPACE 253 <= s" key constants too big" assert_msg
LV_KEY_ENTER 253 <= s" key constants too big" assert_msg
LV_KEY_NEXT 253 <= s" key constants too big" assert_msg
LV_KEY_PREV 253 <= s" key constants too big" assert_msg
LV_KEY_HOME 253 <= s" key constants too big" assert_msg
LV_KEY_END 253 <= s" key constants too big" assert_msg

here constant mapping

: m
	4 allot
	5 1 do
		here i - c!
	loop
;

: u dup 32 - ;
: d2 dup dup ;
: ud2 u d2 ;
: d3 dup 2dup ;

LV_KEY_ESC d3 m                'q' u '1' '!' m   'w' u '2' '@' m   'e' u '3' '#' m   'r' u '4' '$' m   't' u '5' '%' m   'y' u '6' '^' m    'u' u '7' '&' m   'i' u '8' '*' m   'o' u '9' '(' m   'p' u '0' ')' m   LV_KEY_BACKSPACE LV_KEY_DEL d2 m
LV_KEY_NEXT LV_KEY_PREV d2 m   'a' u '`' '~' m   's' ud2 m         'd' ud2 m         'f' ud2 m         'g' u '-' dup m   'h' u '=' '+' m    'j' u '[' '{' m   'k' u ']' '}' m   'l' u '\' '|' m   ';' ':' ''' '"' m '\n' d3 m
shpl d3 m                      0 d3 m            'z' ud2 m         'x' ud2 m         'c' ud2 m         'v' ud2 m         'b' u ',' '<' m    'n' u '.' '>' m   'm' u '/' '?' m   LV_KEY_UP d3 m    0 d3 m            shpl d3 m
sh d3 m                        0 d3 m            0 d3 m            sp d3 m           sp d3 m           sp d3 m           sp d3 m            LV_KEY_LEFT d3 m  LV_KEY_DOWN d3 m  LV_KEY_RIGHT d3 m 0 d3 m            sh d3 m
LV_KEY_UP d3 m LV_KEY_DOWN d3 m LV_KEY_LEFT d3 m LV_KEY_RIGHT d3 m LV_KEY_ENTER d3 m

53 constant key_cnt
here mapping - key_cnt 4 * = s" bad mapping length" assert_msg

mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

here constant buf
1 allot

s" /dev/ukeyboard" drop O_WRONLY 0 open
dup 0 >= s" open ukeyboard failure" assert_msg constant fd

align here constant event
keyboard_event_s allot
event keyboard_event_s 0 fill

variable sh_cnt 0 sh_cnt !
variable shpl_cnt 0 shpl_cnt !
variable sp_cnt 0 sp_cnt !

: cnt_chk @ if 1 else 0 then ;
: cnt_upd
	dup @
	rot if 1- else 1+ then
	dup 0 >= s" cnt underflow" assert_msg
	swap !
;

: proc
	event keyboard_event_s.code + !
	event keyboard_event_s.type + !
	fd event keyboard_event_s write keyboard_event_s = s" bad retval of `write`" assert_msg
;

begin
	con buf 1 mcpd_read
	buf c@
	dup 1 and
	swap 1 rshift
	dup key_cnt < s" invalid key value" assert_msg
	2 lshift mapping + sh_cnt cnt_chk + shpl_cnt cnt_chk 1 lshift + c@
	dup 0= if 2drop else
	dup sh = if drop sh_cnt cnt_upd else
	dup shpl = if drop shpl_cnt cnt_upd else
	dup sp = if
		drop
		dup 0= sp_cnt @ 0= and if dup sp proc then
		dup sp_cnt cnt_upd
		dup 0<> sp_cnt @ 0= and if dup sp proc then
		drop
	else
	proc
	then then then then
	depth 0= s" stack not empty" assert_msg
again
