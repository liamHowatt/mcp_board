.DEFAULT_GOAL := nop

HELPER := ../stm32_helper/stm32_helper.py

include ../stm32_helper/helper.mk

post_cube: fs_bin.c
build: fs_bin.c
clean_cube: clean

fs_bin.c: main.4th lv_disp_driver.4th
	make -C ../heatshrink heatshrink
	mkdir -p compressed
	../heatshrink/heatshrink -w 10 -l 4  lv_disp_driver.4th compressed/lv_disp_driver.4th
	../heatshrink/heatshrink -w 10 -l 4  main.4th compressed/main.4th
	make -C ../mcp_fs/mk_mcp_fs mk_mcp_fs
	../mcp_fs/mk_mcp_fs/mk_mcp_fs 2048 4 tmp compressed/main.4th compressed/lv_disp_driver.4th
	echo 'const unsigned char fs_bin[2048 * 4] __attribute__((aligned(2048))) __attribute__((section(".flashend"))) = {' > fs_bin.c
	cat tmp | xxd -i >> fs_bin.c
	echo '};' >> fs_bin.c
	rm -rf tmp compressed/

clean:
	make -C ../mcp_fs/mk_mcp_fs clean
	rm -rf fs_bin.c tmp compressed/

nop:
	echo please specify target
	false
