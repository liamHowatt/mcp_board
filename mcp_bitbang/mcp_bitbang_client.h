#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MBB_CLI_BYTE_TRANSFER_READ 0xff
#define MBB_CLI_BYTE_TRANSFER_WRITE(byte) (byte)

typedef uint8_t mbb_cli_transfer_t;

typedef enum {
    MBB_CLI_STATUS_DONE,
    MBB_CLI_STATUS_WAIT_CLK_PIN_HIGH,
    MBB_CLI_STATUS_DO_DELAY,
    MBB_CLI_STATUS_INVALID
} mbb_cli_status_t;

typedef enum {
    MBB_CLI_PIN_CLK = 0,
    MBB_CLI_PIN_DAT
} mbb_cli_pin_t;

typedef bool (*mbb_cli_read_cb_t)(void * caller_ctx, mbb_cli_pin_t pin);
typedef void (*mbb_cli_write_cb_t)(void * caller_ctx, mbb_cli_pin_t pin, bool val);

typedef struct {
    mbb_cli_read_cb_t read_cb;
    mbb_cli_write_cb_t write_cb;
    void * caller_ctx;
    uint8_t data_in;
    uint8_t data_out;
    uint8_t bit_progress;
    uint8_t byte_progress;
} mbb_cli_t;

void mbb_cli_init(mbb_cli_t * mbb, mbb_cli_read_cb_t read_cb, mbb_cli_write_cb_t write_cb,
                  void * caller_ctx);
void mbb_cli_start_byte_transfer(mbb_cli_t * mbb, mbb_cli_transfer_t transfer);
mbb_cli_status_t mbb_cli_continue_byte_transfer(mbb_cli_t * mbb);
uint8_t mbb_cli_get_read_byte(mbb_cli_t * mbb);
