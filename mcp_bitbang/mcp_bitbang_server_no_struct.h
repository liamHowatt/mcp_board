#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MBB_SRV_BYTE_TRANSFER_READ -1
#define MBB_SRV_BYTE_TRANSFER_WRITE(byte) (byte)

typedef int16_t mbb_srv_transfer_t;

typedef enum {
    MBB_SRV_PIN_A = 0,
    MBB_SRV_PIN_B = 1
} mbb_srv_pin_t;

typedef struct mbb_srv_t mbb_srv_t;

typedef bool (*mbb_srv_read_cb_t)(void * caller_ctx, mbb_srv_pin_t pin);
typedef void (*mbb_srv_write_cb_t)(void * caller_ctx, mbb_srv_pin_t pin, bool val);

typedef struct {
	mbb_srv_read_cb_t read;
	mbb_srv_write_cb_t write;
} mbb_srv_cbs_t;

void mbb_srv_init(mbb_srv_t * mbb, const mbb_srv_cbs_t * cbs, void * caller_ctx);
void mbb_srv_start_byte_transfer(mbb_srv_t * mbb, mbb_srv_transfer_t transfer);
bool mbb_srv_continue_byte_transfer(mbb_srv_t * mbb);
uint8_t mbb_srv_get_read_byte(mbb_srv_t * mbb);
bool mbb_srv_is_flipped(mbb_srv_t * mbb);
