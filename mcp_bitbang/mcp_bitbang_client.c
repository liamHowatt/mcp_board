#include "mcp_bitbang_client.h"

void mbb_cli_init(mbb_cli_t * mbb, mbb_cli_read_cb_t read_cb, mbb_cli_write_cb_t write_cb,
                  void * caller_ctx)
{
    mbb->read_cb = read_cb;
    mbb->write_cb = write_cb;
    mbb->caller_ctx = caller_ctx;
}

void mbb_cli_start_byte_transfer(mbb_cli_t * mbb, mbb_cli_transfer_t transfer)
{
    mbb->data_in = 0;
    mbb->data_out = transfer;
    mbb->bit_progress = 0;
    mbb->byte_progress = 0;
}

mbb_cli_status_t mbb_cli_continue_byte_transfer(mbb_cli_t * mbb)
{
    if(mbb->byte_progress >= 8) return MBB_CLI_STATUS_DONE;
    mbb_cli_status_t ret = MBB_CLI_STATUS_INVALID;
    switch(mbb->bit_progress) {
        case 0:
            mbb->write_cb(mbb->caller_ctx, MBB_CLI_PIN_CLK, 1);
            if(0 == mbb->read_cb(mbb->caller_ctx, MBB_CLI_PIN_CLK)) {
                ret = MBB_CLI_STATUS_WAIT_CLK_PIN_HIGH;
            }
            else {
                mbb->write_cb(mbb->caller_ctx, MBB_CLI_PIN_DAT, mbb->data_out & (1 << (7 - mbb->byte_progress)));
                ret = MBB_CLI_STATUS_DO_DELAY;
                mbb->bit_progress += 1;
            }
            break;
        case 1:
            mbb->write_cb(mbb->caller_ctx, MBB_CLI_PIN_CLK, 0);
            ret = MBB_CLI_STATUS_DO_DELAY;
            mbb->bit_progress += 1;
            break;
        case 2: 
            mbb->data_in = (mbb->data_in << 1) | mbb->read_cb(mbb->caller_ctx, MBB_CLI_PIN_DAT);
            mbb->write_cb(mbb->caller_ctx, MBB_CLI_PIN_CLK, 1);
            mbb->byte_progress += 1;
            if(mbb->byte_progress >= 8) {
                ret = MBB_CLI_STATUS_DONE;
            }
            else {
                ret = MBB_CLI_STATUS_DO_DELAY_AND_WAIT_CLK_PIN_HIGH;
            }
            mbb->bit_progress = 0;
            break;
    }
    return ret;
}

uint8_t mbb_cli_get_read_byte(mbb_cli_t * mbb)
{
    return mbb->data_in;
}
