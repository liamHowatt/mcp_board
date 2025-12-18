#include "mcp_module_stm32_mcp_fs.h"

#include <assert.h>

#ifdef FLASH_BANK_2
#warning Multiple flash banks are present. Assuming FLASH_BANK_1.
#endif

#define PAGE_ADDR(page_id) (((volatile uint8_t *)FLASH_BASE) + (FLASH_PAGE_SIZE * (page_id)))

static int read_block(void * cb_ctx, int block_index, void * vdst)
{
    mcp_module_stm32_mcp_fs_t * mmfs = cb_ctx;

    uint8_t * dst = vdst;
    volatile uint8_t * src_block = PAGE_ADDR(mmfs->block0_page_id + block_index);

    uint64_t * dst_u64 = (uint64_t *) dst;
    volatile uint64_t * src_block_u64 = (volatile uint64_t *) src_block;

    for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
        *dst_u64++ = *src_block_u64++;
    }

    return 0;
}

static int write_block(void * cb_ctx, int block_index, const void * vsrc)
{
    HAL_StatusTypeDef res;

    mcp_module_stm32_mcp_fs_t * mmfs = cb_ctx;

    res = HAL_FLASH_Unlock();
    if(res != HAL_OK) return -1;

    FLASH_EraseInitTypeDef erase_dsc = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_1,
        .Page = mmfs->block0_page_id + block_index,
        .NbPages = 1
    };
    uint32_t page_error;
    res = HAL_FLASHEx_Erase(&erase_dsc, &page_error);
    if(res != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }

    volatile uint8_t * dst_block = PAGE_ADDR(mmfs->block0_page_id + block_index);
    uint64_t * src = (uint64_t *) vsrc;

    for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
        res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
            (uint32_t) dst_block + i, *src++);
        if(res != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
    }

    res = HAL_FLASH_Lock();
    if(res != HAL_OK) return -1;

    return 0;
}

int mcp_module_stm32_mcp_fs_init(
    mcp_module_stm32_mcp_fs_t * mmfs,
    void * aligned_aux_memory,
    int first_block_page_number,
    int block_count
)
{
    mmfs->conf.aligned_aux_memory = aligned_aux_memory;
    mmfs->conf.block_size = FLASH_PAGE_SIZE;
    mmfs->conf.block_count = block_count;
    mmfs->conf.cb_ctx = mmfs;
    mmfs->conf.read_block = read_block;
    mmfs->conf.write_block = write_block;

    mmfs->block0_page_id = first_block_page_number;

    return mfs_mount(&mmfs->mfs, &mmfs->conf);
}

int mcp_module_stm32_mcp_fs_heatshrink_init(
    mcp_module_stm32_mcp_fs_heatshrink_t * mmfs,
    void * aligned_aux_memory,
    int first_block_page_number,
    int block_count
)
{
    return mcp_module_stm32_mcp_fs_init(&mmfs->base, aligned_aux_memory, first_block_page_number, block_count);
}

static mcp_module_rw_fs_result_t translate_error(int mfs_error)
{
    switch(mfs_error) {
        case 0: return MCP_MODULE_RW_FS_RESULT_OK;
        case MFS_FILE_NOT_FOUND_ERROR: return MCP_MODULE_RW_FS_RESULT_ENOENT;
        case MFS_FILE_NAME_BAD_LEN_ERROR: return MCP_MODULE_RW_FS_RESULT_ENAMETOOLONG;
        case MFS_NO_SPACE_ERROR: return MCP_MODULE_RW_FS_RESULT_ENOSPC;
        default: return MCP_MODULE_RW_FS_RESULT_EIO;
    }
}

static mcp_module_rw_fs_result_t op_list(void * vmmfs, void * cb_ctx, void (*cb)(void *, const char *))
{
    mcp_module_stm32_mcp_fs_t * mmfs = vmmfs;
    int res = mfs_list_files(&mmfs->mfs, cb_ctx, cb);
    return translate_error(res);
}

static mcp_module_rw_fs_result_t op_delete(void * vmmfs, const char *fname)
{
    mcp_module_stm32_mcp_fs_t * mmfs = vmmfs;
    int res = mfs_delete(&mmfs->mfs, fname);
    return translate_error(res);
}

static mcp_module_rw_fs_result_t op_open(void * vmmfs, bool is_read, const char *fname)
{
    mcp_module_stm32_mcp_fs_t * mmfs = vmmfs;
    int res = mfs_open(&mmfs->mfs, fname, is_read ? MFS_MODE_READ : MFS_MODE_WRITE);
    return translate_error(res);
}

static mcp_module_rw_fs_result_t op_close(void * vmmfs)
{
    mcp_module_stm32_mcp_fs_t * mmfs = vmmfs;
    int res = mfs_close(&mmfs->mfs);
    return translate_error(res);
}

static mcp_module_rw_fs_result_t op_read(void * vmmfs, void * dst, uint32_t size, uint32_t * actually_read)
{
    mcp_module_stm32_mcp_fs_t * mmfs = vmmfs;
    int res = mfs_read(&mmfs->mfs, dst, size);
    if(res < 0) return translate_error(res);
    *actually_read = res;
    return MCP_MODULE_RW_FS_RESULT_OK;
}

static mcp_module_rw_fs_result_t op_write(void * vmmfs, const void * src, uint32_t size)
{
    mcp_module_stm32_mcp_fs_t * mmfs = vmmfs;
    int res = mfs_write(&mmfs->mfs, src, size);
    if(res < 0) return translate_error(res);
    return MCP_MODULE_RW_FS_RESULT_OK;
}

static mcp_module_rw_fs_result_t op_open_heatshrink(void * vmmfs, bool is_read, const char *fname)
{
    mcp_module_stm32_mcp_fs_heatshrink_t * mmfs = vmmfs;
    mmfs->is_read = is_read;
    if(is_read) {
        mmfs->buf_len = 0;
        heatshrink_decoder_reset(&mmfs->decoder);
    } else {
        mmfs->write_needs_finalize = true;
        heatshrink_encoder_reset(&mmfs->encoder);
    }
    return op_open(vmmfs, is_read, fname);
}

static mcp_module_rw_fs_result_t op_close_heatshrink(void * vmmfs)
{
    int res;
    size_t hs_actual;
    mcp_module_stm32_mcp_fs_heatshrink_t * mmfs = vmmfs;

    mcp_module_rw_fs_result_t write_res = MCP_MODULE_RW_FS_RESULT_OK;

    if(!mmfs->is_read && mmfs->write_needs_finalize) {
        while(1) {
            HSE_finish_res finish_res = heatshrink_encoder_finish(&mmfs->encoder);
            assert(finish_res >= 0);

            if(finish_res == HSER_FINISH_DONE) break;

            HSE_poll_res poll_res = heatshrink_encoder_poll(
                &mmfs->encoder,
                mmfs->buf,
                sizeof(mmfs->buf),
                &hs_actual
            );
            assert(poll_res >= 0);

            res = mfs_write(&mmfs->base.mfs, mmfs->buf, hs_actual);
            if(res < 0) {
                write_res = translate_error(res);
                break;
            }
        }
    }

    mcp_module_rw_fs_result_t close_res = op_close(vmmfs);
    return write_res ? write_res : close_res;
}

static mcp_module_rw_fs_result_t op_read_heatshrink(void * vmmfs, void * dst, uint32_t size, uint32_t * actually_read)
{
    int res;
    size_t hs_actual;
    mcp_module_stm32_mcp_fs_heatshrink_t * mmfs = vmmfs;

    uint32_t total_read = 0;
    while(size) {
        while(mmfs->buf_len) {
            HSD_sink_res sink_res = heatshrink_decoder_sink(
                &mmfs->decoder,
                mmfs->buf + mmfs->buf_ofs,
                mmfs->buf_len,
                &hs_actual
            );
            assert(sink_res >= 0);
            mmfs->buf_len -= hs_actual;
            mmfs->buf_ofs += hs_actual;

            if(sink_res == HSDR_SINK_FULL) break;
        }

        while(size) {
            HSD_poll_res poll_res = heatshrink_decoder_poll(
                &mmfs->decoder,
                dst,
                size,
                &hs_actual
            );
            assert(poll_res >= 0);
            size -= hs_actual;
            dst += hs_actual;
            total_read += hs_actual;

            if(poll_res == HSDR_POLL_EMPTY) break;
        }

        if(mmfs->buf_len) continue;

        res = mfs_read(&mmfs->base.mfs, mmfs->buf, sizeof(mmfs->buf));
        if(res < 0) return translate_error(res);
        mmfs->buf_len = res;
        mmfs->buf_ofs = 0;

        if(res == 0) {
            HSD_finish_res finish_res = heatshrink_decoder_finish(&mmfs->decoder);
            assert(finish_res >= 0);

            if(finish_res == HSDR_FINISH_DONE) break;
        }
    }

    *actually_read = total_read;
    return MCP_MODULE_RW_FS_RESULT_OK;
}

static mcp_module_rw_fs_result_t op_write_heatshrink(void * vmmfs, const void * src, uint32_t size)
{
    int res;
    size_t hs_actual;
    mcp_module_stm32_mcp_fs_heatshrink_t * mmfs = vmmfs;

    while(size) {
        HSE_sink_res sink_res = heatshrink_encoder_sink(
            &mmfs->encoder,
            (uint8_t *) src, /* it doesn't write to it despite the parameter not being const */
            size,
            &hs_actual
        );
        assert(sink_res >= 0);
        size -= hs_actual;
        src += hs_actual;

        while(1) {
            HSE_poll_res poll_res = heatshrink_encoder_poll(
                &mmfs->encoder,
                mmfs->buf,
                sizeof(mmfs->buf),
                &hs_actual
            );
            assert(poll_res >= 0);

            res = mfs_write(&mmfs->base.mfs, mmfs->buf, hs_actual);
            if(res < 0) {
                mmfs->write_needs_finalize = false;
                return translate_error(res);
            }

            if(poll_res == HSER_POLL_EMPTY) break;
        }
    }

    return MCP_MODULE_RW_FS_RESULT_OK;
}

const mcp_module_rw_fs_vtable_t mcp_module_stm32_mcp_fs_rw_fs_vtable = {
    .list = op_list,
    .delete = op_delete,
    .open = op_open,
    .close = op_close,
    .read = op_read,
    .write = op_write
};

const mcp_module_rw_fs_vtable_t mcp_module_stm32_mcp_fs_heatshrink_rw_fs_vtable = {
    .list = op_list,
    .delete = op_delete,
    .open = op_open_heatshrink,
    .close = op_close_heatshrink,
    .read = op_read_heatshrink,
    .write = op_write_heatshrink
};
