#include "mcp_bitbang_server.h"

void mbb_srv_init(mbb_srv_t * mbb, const mbb_srv_cbs_t * cbs, void * caller_ctx)
{
    mbb->cbs = cbs;
    mbb->caller_ctx = caller_ctx;
    mbb->bootstrapping = true;
    mbb->bootstrap_progress = 0;
    mbb->clk_pin = 0;
}

void mbb_srv_start_byte_transfer(mbb_srv_t * mbb, mbb_srv_transfer_t transfer)
{
    mbb->is_read = transfer < 0;
    mbb->data = transfer >= 0 ? transfer : 0;
    mbb->bit_progress = 0;
    mbb->byte_progress = 0;
}

bool mbb_srv_continue_byte_transfer(mbb_srv_t * mbb)
{
    if(mbb->bootstrapping) {
        switch(mbb->bootstrap_progress) {
            case 0:
                mbb->cbs->write(mbb->caller_ctx, MBB_SRV_PIN_A, 1);
                mbb->cbs->write(mbb->caller_ctx, MBB_SRV_PIN_B, 1);
                mbb->bootstrap_progress += 1;
                return false;
            case 1: {
                bool a_low = 0 == mbb->cbs->read(mbb->caller_ctx, MBB_SRV_PIN_A);
                if(a_low || 0 == mbb->cbs->read(mbb->caller_ctx, MBB_SRV_PIN_B)) {
                    mbb->clk_pin = !a_low;
                    mbb->bootstrapping = false;
                    break;
                }
                return false;
            }
        }
    }

    if(mbb->byte_progress >= 8) return true;
    bool ret = false;
    switch(mbb->bit_progress) {
        case 0:
            mbb->cbs->write(mbb->caller_ctx, mbb->clk_pin, 1);
            mbb->bit_progress += 1;
            break;
        case 1:
            if(0 != mbb->cbs->read(mbb->caller_ctx, mbb->clk_pin)) {
                if(mbb->is_read) mbb->cbs->write(mbb->caller_ctx, !mbb->clk_pin, 1);
                mbb->bit_progress += 1;
            }
            break;
        case 2:
            if(0 == mbb->cbs->read(mbb->caller_ctx, mbb->clk_pin)) {
                if(mbb->is_read) {
                    mbb->data = (mbb->data << 1) | mbb->cbs->read(mbb->caller_ctx, !mbb->clk_pin);
                } else {
                    mbb->cbs->write(mbb->caller_ctx, !mbb->clk_pin, mbb->data & (1 << (7 - mbb->byte_progress)));
                }
                mbb->byte_progress += 1;
                ret = mbb->byte_progress >= 8;
                if(ret) mbb->cbs->write(mbb->caller_ctx, mbb->clk_pin, 0);
                mbb->bit_progress = 0;
            }
            break;
    }
    return ret;
}

uint8_t mbb_srv_get_read_byte(mbb_srv_t * mbb)
{
    return mbb->data;
}

bool mbb_srv_is_flipped(mbb_srv_t * mbb)
{
    return mbb->clk_pin;
}
