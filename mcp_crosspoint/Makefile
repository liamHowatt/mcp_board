crosspoint.bin: crosspoint.v
	/home/liam/oss-cad-suite/bin/yosys -p "synth_ice40 -top crosspoint -json crosspoint.json" crosspoint.v
	/home/liam/oss-cad-suite/bin/nextpnr-ice40 --hx8k --package tq144:4k --json crosspoint.json --pcf crosspoint.pcf --asc crosspoint.asc
	/home/liam/oss-cad-suite/bin/icepack crosspoint.asc crosspoint.bin

crosspoint_bin_compressed: crosspoint.bin
	make -C ../base_fpga_flasher/heatshrink heatshrink
	../base_fpga_flasher/heatshrink/heatshrink -e -w 8 -l 4 crosspoint.bin crosspoint_bin_compressed

crosspoint_bin_compressed.c: crosspoint_bin_compressed
	echo -n 'const ' > crosspoint_bin_compressed.c
	xxd -i crosspoint_bin_compressed >> crosspoint_bin_compressed.c
