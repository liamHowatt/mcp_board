.DEFAULT_GOAL := nop

HELPER := ../stm32_helper/stm32_helper.py

include ../stm32_helper/helper.mk

post_cube: fs_bin.c main_4th_gz.h
build:     fs_bin.c main_4th_gz.h
clean_cube: clean

mac.bin:
	echo 'create a mac.bin file with the MAC address for this module.'
	echo 'e.g. for 02:cb:d7:aa:86:03, run `echo -ne "\x02\xcb\xd7\xaa\x86\x03" > mac.bin`'
	false

fs_bin.c: mac.bin
	make -C ../mcp_fs/mk_mcp_fs mk_mcp_fs
	../mcp_fs/mk_mcp_fs/mk_mcp_fs --mini 2048 1 tmp mac.bin
	echo 'const unsigned char fs_bin[2048 * 1] __attribute__((aligned(2048))) = {' > fs_bin.c
	cat tmp | xxd -i >> fs_bin.c
	echo '};' >> fs_bin.c
	rm tmp

# google search "zopfli" if gzip compression is not enough
main.4th.gz: main.4th
	gzip --keep --no-name --force main.4th

main_4th_gz.h: main.4th.gz
	echo 'const unsigned char main_4th_gz[] = {' > main_4th_gz.h
	cat main.4th.gz | xxd -i >> main_4th_gz.h
	echo '};' >> main_4th_gz.h

clean:
	make -C ../mcp_fs/mk_mcp_fs clean
	rm -f fs_bin.c main.4th.gz main_4th_gz.h tmp

nop:
	echo please specify target
	false
