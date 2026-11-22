here 6 allot constant macaddr
\ read our mac address from the mac.bin file
s" mac.bin" drop mcpd_acatpath
dup s" acatpath fail" assert_msg
( path )
dup O_RDONLY 0 open
dup 0 >= s" mac.bin open failed" assert_msg
( path fd )
swap free
( fd )
dup macaddr 6 read
6 = s" mac.bin read fail" assert_msg
close
0= s" mac.bin close fail" assert_msg


mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

here 3 allot constant cbuf
here 2 allot constant sbuf

CONFIG_NET_TUN_PKTSIZE malloc
dup s" malloc failed" assert_msg
constant lbuf

con sbuf 1 mcpd_read
sbuf c@ constant socketno

con MCP_PINS_PERIPH_TYPE_SPI MCP_PINS_DRIVER_TYPE_SPI_RAW mcpd_resource_acquire
dup 0 >= s" mcpd_resource_acquire failure" assert_msg constant spi_id

con MCP_PINS_PERIPH_TYPE_GPIOIN MCP_PINS_DRIVER_TYPE_GPIOIN_RAW mcpd_resource_acquire
dup 0 >= s" mcpd_resource_acquire failure" assert_msg constant gpioin_id

: route_spi ( io_type pinno -- )
	>r >r con spi_id r> socketno r>
	( con  spi_id  io_type  socketno  pinno )
	mcpd_resource_route 0= s" spi mcpd_resource_route failure" assert_msg
;

MCP_PINS_PIN_SPI_CLK  2 route_spi
MCP_PINS_PIN_SPI_MOSI 0 route_spi
MCP_PINS_PIN_SPI_CS   1 route_spi
MCP_PINS_PIN_SPI_MISO 3 route_spi

con gpioin_id MCP_PINS_PIN_GPIOIN_PIN socketno 2 mcpd_resource_route
0= s" gpioin mcpd_resource_route failure" assert_msg

con spi_id mcpd_resource_get_path
dup s" spi get_path failure" assert_msg
O_RDONLY 0 open
dup -1 <> s" spi open failure" assert_msg constant sfd

con socketno 3 mcpd_gpio_acquire
dup 0 >= s" mcpd_gpio_acquire failure" assert_msg
constant rst_gpio_id

con rst_gpio_id 0 mcpd_gpio_set \ should already be the case
1 ms \ 500us is enough according to DS
con rst_gpio_id 1 mcpd_gpio_set
100 ms

con gpioin_id mcpd_resource_get_path
dup s" gpio get_path failure" assert_msg
( path )
mcpd_gpioin_open
dup s" gpioin open failure" assert_msg
constant gpioin

: callot ( size -- ptr )
	align
	here over allot
	dup rot 0 fill
;

spi_trans_s 2* callot
dup constant trans0
spi_trans_s + constant trans1
spi_sequence_s callot constant seq

mcpd_peer_id seq spi_sequence_s.dev + !
8        seq spi_sequence_s.nbits + c!
2        seq spi_sequence_s.ntrans + c!
trans0   seq spi_sequence_s.trans + !
10000000 seq spi_sequence_s.frequency + !

3    trans0 spi_trans_s.nwords + !
cbuf trans0 spi_trans_s.txbuffer + !

\ block-select bits
: SnREG ( n -- SnREG-bsb ) 5 lshift 0x08 or ;
0 constant COM
0 SnREG constant S0REG
0x10 constant S0TX
0x18 constant S0RX

: bew! ( be16 dst -- )
	over 8 rshift over c!
	1+ c!
;

: rcv_snd_inner ( addr16 len -- )
	trans1 spi_trans_s.nwords + !
	cbuf bew!
	sfd SPIIOC_TRANSFER seq ioctl
	-1 <> s" spi ioctl failed" assert_msg
;

: rcv ( addr16 bsb len buf -- )
	trans1 spi_trans_s.rxbuffer + !
	0 trans1 spi_trans_s.txbuffer + !
	swap cbuf 1+ 1+ c!
	rcv_snd_inner
;
: rcvsb ( addr16 bsb len -- ) sbuf rcv ;
: rcvlb ( addr16 len -- ) S0RX swap lbuf rcv ;
: rcvbyte ( addr16 bsb -- byte ) 1 rcvsb sbuf c@ ;
: rcv16 ( addr16 bsb -- 16 ) 2 rcvsb sbuf c@ 8 lshift sbuf 1+ c@ or ;
: rcv16atomic ( addr16 bsb -- 16 )
	2dup rcv16 >r
	begin
		( addr16 bsb  R: last16 )
		2dup rcv16
		dup r> swap >r
		( addr16 bsb new16 last16  R: new16 )
		=
	until
	2drop
	r>
;

: snd ( addr16 bsb len buf -- )
	0 trans1 spi_trans_s.rxbuffer + !
	trans1 spi_trans_s.txbuffer + !
	swap 0x04 or cbuf 1+ 1+ c! \ set the write bit in the control byte
	rcv_snd_inner
;
: sndsb ( addr16 bsb len -- ) sbuf snd ;
: sndlb ( addr16 len -- ) S0TX swap lbuf snd ;
: sndbyte ( addr16 bsb byte -- ) sbuf c! 1 sndsb ;
: snd16 ( addr16 bsb 16 -- ) sbuf bew! 2 sndsb ;

\ sanity check the w5500 IC version
0x0039 COM rcvbyte 0x04 = s" chip version is not 0x04" assert_msg

\ set our mac address so MAC filtering can work
0x0009 COM 6 macaddr snd \ set "source hardware address"

\ set 0kB buffers for sockets [1, 7]
8 1 do
	0x001E i SnREG 0 sndbyte \ RX buffer size
	0x001F i SnREG 0 sndbyte \ TX buffer size
loop
\ set 16kB buffers for socket 0
0x001E S0REG 16 sndbyte \ RX buffer size
0x001F S0REG 16 sndbyte \ TX buffer size

0x0000 S0REG 0x84 sndbyte \ set socket 0 to MACRAW mode with MAC filtering enabled

\ wait for link up (cable plugged in)
0 begin
	if 1000 ms then 1
	0x002E COM rcvbyte \ PHYCFGR
	1 and \ LNK "link status"
until drop

s" /dev/net/tun" drop O_RDWR O_NONBLOCK or 0 open
dup -1 <> s" tun open failed" assert_msg
constant tfd

here IFNAMSIZ allot constant ifname
tfd IFF_TAP IFF_NO_PI or 0 ifname tunsetiff_wrapper
0= s" TUNSETIFF failed" assert_msg

tfd 1 tunsetcarrier_wrapper
0= s" TUNSETCARRIER failed" assert_msg

\ shared data: only reads: ifname macaddr
: thread_cb ( arg -- ret )
	drop

	\ bring up the interface
	ifname netlib_ifup
	0= s" netlib_ifup fail" assert_msg

	\ set the mac address
	ifname macaddr netlib_setmacaddr
	0= s" netlib_setmacaddr fail" assert_msg

	\ perform DHCP aka "renew".
	\ This blocks and is the purpose of this thread.
	ifname netlib_obtain_ipv4addr
	0= s" netlib_obtain_ipv4addr DHCP fail" assert_msg

	0
;

0x0001 S0REG 0x01 sndbyte \ OPEN socket command

\ confirm the status register reflects MACRAW mode
0x0003 S0REG rcvbyte 0x42 = s" MACRAW status not established" assert_msg

variable recv_already_read 0 recv_already_read !
variable rx_rd 0 rx_rd !
variable recv_has_more 0 recv_has_more !
: recvhasmore ( -- hasmore ) recv_has_more @ ;
: recvframe ( -- pktlen )
	0x0026 S0REG rcv16atomic recv_already_read @ - \ S0_RX_RSR socket received size
	( recvamt )
	dup 0= if
		0 recv_has_more !
		exit
	then
	dup 2 >= s" avail RECV amt was less than hdr size" assert_msg
	rx_rd @ S0RX rcv16 >r \ get the packet size
	( recvamt  R: pktlen+2 )
	dup r@ >= s" avail RECV amt was less than pkt size " assert_msg
	r@ > recv_has_more !
	(   R: pktlen+2 )
	r@ 1- 1-
	( pktlen  R: pktlen+2 )
	dup CONFIG_NET_TUN_PKTSIZE <= s" pkt is bigger than TUN buf" assert_msg
	dup 14 >= s" pkt is smaller than eth hdr" assert_msg
	rx_rd @ 1+ 1+ swap ( rx_rd+2 pktlen  R: pktlen+2 ) rcvlb
	(   R: pktlen+2 )

	r@ recv_already_read +!
	r@ rx_rd +!

	r> 1- 1-
;
: recvupdate ( -- )
	recv_already_read @ if
		0x0028 S0REG rx_rd @ snd16 \ update S0_RX_RD
		0x0001 S0REG 0x40 sndbyte \ RECV socket command
		0 recv_already_read !
	then
;

\ this logic copied from nuttx/drivers/net/w5500.c
16 1024 * CONFIG_NET_TUN_PKTSIZE / dup 0> s" tunpkt2big" assert_msg constant NUM_TXBUFS
NUM_TXBUFS 1+ 2* callot constant txbufs
variable tx_rd_idx 0 tx_rd_idx !
variable tx_wr_idx 0 tx_wr_idx !
: send_wrap_idx ( idx -- idxnext ) 1+ NUM_TXBUFS 1+ mod ;
: send_wrap_idx_writeback ( idxptr -- ) dup @ send_wrap_idx swap ! ;
: sendfreecount ( -- count )
	tx_wr_idx @ tx_rd_idx @ >= if
		NUM_TXBUFS tx_wr_idx @ tx_rd_idx @ - -
	else
		tx_rd_idx @ tx_wr_idx @ - 1-
	then
;
: sendpendingcount ( -- count )
	NUM_TXBUFS sendfreecount -
;
: sendframe ( pktlen -- )
	txbufs tx_wr_idx @ 2* + w@
	( pktlen offset )

	2dup swap sndlb

	tx_wr_idx send_wrap_idx_writeback
	+
	( pktlen+offset )
	dup txbufs tx_wr_idx @ 2* + w!

	sendpendingcount 1 = if
		0x0024 S0REG rot snd16 \ update S0_TX_WR
		0x0001 S0REG 0x20 sndbyte \ SEND socket command
	else drop then
;
: sendnext ( -- )
	tx_rd_idx send_wrap_idx_writeback

	sendpendingcount 0= if exit then

	txbufs tx_rd_idx @ send_wrap_idx 2* + w@
	( offset )
	0x0024 S0REG rot snd16 \ update S0_TX_WR
	0x0001 S0REG 0x20 sndbyte \ SEND socket command
;

pollfd 2* callot
dup constant poll0
pollfd + constant poll1

: pollsinglenonblock ( fd events -- ready )
	dup >r
	poll0 pollfd.events + !
	poll0 pollfd.fd + !
	poll0 1 0 poll
	dup if
		dup 1 = s" single poll err" assert_msg
		poll0 pollfd.revents + @ r> = s" single poll revents err" assert_msg
	else
		r> drop
	then
;

: tunread ( -- pktlen )
	tfd lbuf CONFIG_NET_TUN_PKTSIZE read
	dup 0> s" tun read err" assert_msg
;

: tunwrite ( pktlen -- )
	tfd lbuf 2 pick write
	= s" tun write err" assert_msg
;

1 constant TUN_EVENT
2 constant WIZ_EVENT
3 constant BOTH_EVENTS

TUN_EVENT constant transmitter_sm_initial_want
variable tx_wiz_pending 0 tx_wiz_pending ! \ sm only clears it
: transmitter_sm ( events -- wants )
	dup WIZ_EVENT and if sendpendingcount if
		0 tx_wiz_pending !
		sendnext
	then then

	TUN_EVENT and if sendfreecount if
		tunread sendframe
	then then

	( )

	sendpendingcount
	dup if drop WIZ_EVENT then

	sendfreecount
	dup if drop TUN_EVENT then

	( 0|WIZ_EVENT 0|TUN_EVENT )
	or
;

BOTH_EVENTS constant receiver_sm_initial_want
variable rx_wiz_pending 0 rx_wiz_pending ! \ sm only clears it
: receiver_sm ( events -- wants )
	dup WIZ_EVENT = if drop TUN_EVENT exit then
	dup TUN_EVENT = if drop WIZ_EVENT exit then

	begin drop
		0 rx_wiz_pending !
		recvframe ?dup if
			tunwrite
		\ else
		\ 	." [eth0] warning: spurious wake for receiver" cr
		then

		recvhasmore 0=
		dup if drop WIZ_EVENT then

		tfd POLLOUT pollsinglenonblock 0=
		dup if drop TUN_EVENT then

		( 0|WIZ_EVENT 0|TUN_EVENT )
		or dup
	until

	recvupdate
;

gpioin MCPD_GPIOIN_INTERRUPT_MODE_FALLING mcpd_gpioin_set_interrupt_mode
0x002C S0REG 0x14 sndbyte \ set interrupt mask to RECV and SEND_OK
0x0018 COM 0x01 sndbyte \ enable interrupts on socket 0

0 0 ' thread_cb thread-create drop \ leak thread_hdl

variable tx_wants transmitter_sm_initial_want tx_wants !
variable rx_wants receiver_sm_initial_want rx_wants !
variable tx_has
variable rx_has
begin
	tx_wants @ rx_wants @ or TUN_EVENT and if
		tfd poll0 pollfd.fd + !
		0
		tx_wants @ TUN_EVENT and if POLLIN or then
		rx_wants @ TUN_EVENT and if POLLOUT or then
		poll0 pollfd.events + !
	else
		-1 poll0 pollfd.fd + !
	then
	tx_wants @ rx_wants @ or WIZ_EVENT and if
		gpioin mcpd_gpioin_get_interrupt_polling_fd poll1 pollfd.fd + !
		POLLIN poll1 pollfd.events + !
	else
		-1 poll1 pollfd.fd + !
	then

	\ tx_wants @ . rx_wants @ . cr

	poll0 2 -1 poll
	0> s" poll err" assert_msg

	0 tx_has !
	0 rx_has !

	poll0 pollfd.fd + @ 0 >= if poll0 pollfd.revents + @ ?dup if
		dup POLLIN POLLOUT or invert and 0= s" poll tun unexpected revents" assert_msg
		dup POLLIN  and if tx_has @ TUN_EVENT or tx_has ! then
		    POLLOUT and if rx_has @ TUN_EVENT or rx_has ! then
	then then

	poll1 pollfd.fd + @ 0 >= if poll1 pollfd.revents + @ ?dup if
		POLLIN = s" poll wiznet unexpected revents" assert_msg
		\ ." INT!" cr
		gpioin mcpd_gpioin_consume_interrupts 0= if ." [eth0] spurius interrupt" cr then
		0x0002 S0REG 2dup rcvbyte dup >r sndbyte r> \ clear it
		dup 0x10 and if WIZ_EVENT tx_wiz_pending ! then
		    0x04 and if WIZ_EVENT rx_wiz_pending ! then
	then then
	tx_has @ tx_wiz_pending @ or tx_has !
	rx_has @ rx_wiz_pending @ or rx_has !

	\ tx_has @ . rx_has @ . cr
	\ cr

	tx_has @ tx_wants @ and if
		tx_has @ transmitter_sm tx_wants !
	then

	rx_has @ rx_wants @ and if
		rx_has @ receiver_sm rx_wants !
	then
again

\ gpioin mcpd_gpioin_close
\ sfd close drop
\ tfd close drop
\ thread_hdl thread_join drop
\ lbuf free
\ con mcpd_disconnect
