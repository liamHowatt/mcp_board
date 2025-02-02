test: mcp_modnet_server.c mcp_modnet_server_test.c mcp_modnet_server_bitbang_include.h mcp_modnet_server_test_bitbang.h mcp_modnet_server.h ../mcp_bitbang/mcp_bitbang_server_no_struct.h
	gcc -Wall -g -fsanitize=address -DMMN_SRV_TEST -o test mcp_modnet_server.c mcp_modnet_server_test.c
