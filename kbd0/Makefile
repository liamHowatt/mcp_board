.DEFAULT_GOAL := nop

HELPER := ../stm32_helper/stm32_helper.py

include ../stm32_helper/helper.mk

post_cube: fs_bin.c
build: fs_bin.c
clean_cube: clean

fs_bin.c: main.4th
	make -C ../mcp_fs/mk_mcp_fs mk_mcp_fs
	../mcp_fs/mk_mcp_fs/mk_mcp_fs 2048 50 tmp main.4th
	echo 'const unsigned char fs_bin[2048 * 50] __attribute__((aligned(2048))) = {' > fs_bin.c
	cat tmp | xxd -i >> fs_bin.c
	echo '};' >> fs_bin.c
	rm tmp

clean:
	make -C ../mcp_fs/mk_mcp_fs clean
	rm -f fs_bin.c tmp

nop:
	echo please specify target
	false
