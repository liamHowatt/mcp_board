#pragma once

#include "mcp_bitbang_server_no_struct.h"

#include <stdint.h>
#include <stdbool.h>

struct mbb_srv_t {
    const mbb_srv_cbs_t * cbs;
    void *  caller_ctx;
    bool    bootstrapping;
    uint8_t bootstrap_progress;
    uint8_t clk_pin;
    bool    is_read;
    uint8_t data;
    uint8_t bit_progress;
    uint8_t byte_progress;
};
