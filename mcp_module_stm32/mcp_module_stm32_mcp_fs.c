#include "mcp_module_stm32_mcp_fs.h"

#ifdef FLASH_BANK_2
#warning Multiple flash banks are present. Assuming FLASH_BANK_1.
#endif

#define PAGE_ADDR(page_id) (((volatile uint8_t *)FLASH_BASE) + (FLASH_PAGE_SIZE * (page_id)))

static int read_block(void * cb_ctx, int block_index)
{
    mcp_module_stm32_mcp_fs_t * mmfs = cb_ctx;

    uint8_t * dst = mmfs->conf.block_buf;
    volatile uint8_t * src_block = PAGE_ADDR(mmfs->block0_page_id + block_index);

    uint64_t * dst_u64 = (uint64_t *) dst;
    volatile uint64_t * src_block_u64 = (volatile uint64_t *) src_block;

    for(uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 8) {
        *dst_u64++ = *src_block_u64++;
    }

    return 0;
}

static int write_block(void * cb_ctx, int block_index)
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
    uint64_t * src = (uint64_t *) mmfs->conf.block_buf;

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
    mmfs->conf.block_buf = aligned_aux_memory;
    uint8_t * aux_mem_u8 = aligned_aux_memory;
    aux_mem_u8 += FLASH_PAGE_SIZE;
    int bit_buf_size = MFS_BIT_BUF_SIZE_BYTES(block_count);
    for(int i = 0; i < 4; i++) {
        mmfs->conf.bit_bufs[i] = aux_mem_u8;
        aux_mem_u8 += bit_buf_size;
    }
    mmfs->conf.block_size = FLASH_PAGE_SIZE;
    mmfs->conf.block_count = block_count;
    mmfs->conf.cb_ctx = mmfs;
    mmfs->conf.read_block = read_block;
    mmfs->conf.write_block = write_block;

    mmfs->block0_page_id = first_block_page_number;

    return mfs_mount(&mmfs->mfs, &mmfs->conf);
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

const mcp_module_rw_fs_vtable_t mcp_module_stm32_mcp_fs_rw_fs_vtable = {
    .list = op_list,
    .delete = op_delete,
    .open = op_open,
    .close = op_close,
    .read = op_read,
    .write = op_write
};
