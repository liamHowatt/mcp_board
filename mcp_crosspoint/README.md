```
python gen_verilog_and_pcf.py
yosys -p "synth_ice40 -top top -json base0.json" base0.v
nextpnr --hx8k --package tq144:4k --json base0.json --pcf base0.pcf --asc base0.asc
icepack base0.asc base0.bin
```
