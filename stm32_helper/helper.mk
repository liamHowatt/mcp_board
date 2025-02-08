


post_cube:
	python $(HELPER) post_cube

build:
	python $(HELPER) build --gcc-path '/home/liam/mcp_board/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux/gcc-arm-none-eabi-10.3-2021.10/bin/'

upload:
	python $(HELPER) upload

reset_mcu:
	python $(HELPER) reset_mcu

debug:
	python $(HELPER) debug --gcc-path '/home/liam/mcp_board/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux/gcc-arm-none-eabi-10.3-2021.10/bin/'

clean_cube:
	python $(HELPER) clean

