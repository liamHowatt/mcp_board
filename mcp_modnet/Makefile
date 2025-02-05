test: test.a sim.a
	g++ -g -fsanitize=address test.a sim.a -pthread -lpthread -latomic -o test

test.a: sim.h mcp_modnet_server.c mcp_modnet_server_test.c mcp_modnet_server_bitbang_include.h mcp_modnet_server_test_bitbang.h mcp_modnet_server.h ../mcp_bitbang/mcp_bitbang_server_no_struct.h
	gcc -Wall -g -fsanitize=address -DMMN_SRV_TEST -c mcp_modnet_server.c mcp_modnet_server_test.c
	ar r test.a mcp_modnet_server.o mcp_modnet_server_test.o

sim.a: sim.cpp ../mcp_crosspoint/crosspoint.v sim.h
	verilator --cc --build -j 4 -Wall sim.cpp ../mcp_crosspoint/crosspoint.v
	cp obj_dir/Vcrosspoint__ALL.a ./sim.a
	ar rs sim.a obj_dir/sim.o obj_dir/verilated.o obj_dir/verilated_threads.o
