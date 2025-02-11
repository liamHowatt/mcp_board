#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include <inttypes.h>
#include <stdint.h>

// #include "fpga_flash.h"
// #include "stm32.h"
#include <string.h>
// #include "fpga_bin.h"
#include <stdbool.h>
#include "heatshrink/heatshrink_decoder.h"

#define CROSSPOINT_UNCOMPRESSED_LEN 135100

extern const unsigned char crosspoint_bin_compressed[];
extern unsigned int crosspoint_bin_compressed_len;

#if !defined(HEATSHRINK_DYNAMIC_ALLOC) || HEATSHRINK_DYNAMIC_ALLOC
#error HEATSHRINK_DYNAMIC_ALLOC shall be 0
#endif

#define FPGA_FLASH_PAGE_SIZE          256
#define FLASH_SECTOR_SIZE_4k         4096
#define FLASH_SECTOR_SIZE_32k       32768
#define FLASH_SECTOR_SIZE_64k       65536

#define FLASH_CMD_PAGE_PROGRAM       0x02
#define FLASH_CMD_READ               0x03
#define FLASH_CMD_STATUS             0x05
#define FLASH_CMD_WRITE_EN           0x06
#define FLASH_CMD_SECTOR_ERASE_4k    0x20
#define FLASH_CMD_SECTOR_ERASE_32k   0x52
#define FLASH_CMD_SECTOR_ERASE_64k   0xD8

#define FLASH_STATUS_BUSY_MASK 0x01

static void a(HAL_StatusTypeDef s) {
    if (s != HAL_OK) {
        while (1);
    }
}

static void cs_select(void) {
    // stm32_microsecond_timer_delay(&MICROSECOND_TIMER, 1);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 0);
    // stm32_microsecond_timer_delay(&MICROSECOND_TIMER, 1);
    HAL_Delay(2);
}

static void cs_deselect(void) {
    // stm32_microsecond_timer_delay(&MICROSECOND_TIMER, 1);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 1);
    // stm32_microsecond_timer_delay(&MICROSECOND_TIMER, 1);
    HAL_Delay(2);
}

static void flash_read(uint32_t addr, uint8_t *buf, size_t len) {
    cs_select();
    uint8_t cmdbuf[4] = {
            FLASH_CMD_READ,
            addr >> 16,
            addr >> 8,
            addr
    };
    a(HAL_SPI_Transmit(&FLASH_SPI, cmdbuf, 4, 1000));
    a(HAL_SPI_Receive(&FLASH_SPI, buf, len, 1000));
    cs_deselect();
}

static void flash_write_enable(void) {
    cs_select();
    uint8_t cmd = FLASH_CMD_WRITE_EN;
    a(HAL_SPI_Transmit(&FLASH_SPI, &cmd, 1, 1000));
    cs_deselect();
}

static void flash_wait_done(void) {
    uint8_t status;
    do {
        cs_select();
        uint8_t buf[2] = {FLASH_CMD_STATUS, 0};
        a(HAL_SPI_TransmitReceive(&FLASH_SPI, buf, buf, 2, 1000));
        cs_deselect();
        status = buf[1];
    } while (status & FLASH_STATUS_BUSY_MASK);
}

static void flash_sector_erase_4k(uint32_t addr) {
    if (addr % FLASH_SECTOR_SIZE_4k) while(1);
    uint8_t cmdbuf[4] = {
            FLASH_CMD_SECTOR_ERASE_4k,
            addr >> 16,
            addr >> 8,
            addr
    };
    flash_write_enable();
    cs_select();
    a(HAL_SPI_Transmit(&FLASH_SPI, cmdbuf, 4, 1000));
    cs_deselect();
    flash_wait_done();
}

static void flash_sector_erase_64k(uint32_t addr) {
    if (addr % FLASH_SECTOR_SIZE_64k) while(1);
    uint8_t cmdbuf[4] = {
            FLASH_CMD_SECTOR_ERASE_64k,
            addr >> 16,
            addr >> 8,
            addr
    };
    flash_write_enable();
    cs_select();
    a(HAL_SPI_Transmit(&FLASH_SPI, cmdbuf, 4, 1000));
    cs_deselect();
    flash_wait_done();
}

static void flash_page_program(uint32_t addr, const uint8_t data[]) {
    if (addr % FPGA_FLASH_PAGE_SIZE) while(1);
    uint8_t cmdbuf[4] = {
            FLASH_CMD_PAGE_PROGRAM,
            addr >> 16,
            addr >> 8,
            addr
    };
    flash_write_enable();
    cs_select();
    a(HAL_SPI_Transmit(&FLASH_SPI, cmdbuf, 4, 1000));
    a(HAL_SPI_Transmit(&FLASH_SPI, (uint8_t *) data, FPGA_FLASH_PAGE_SIZE, 1000));
    cs_deselect();
    flash_wait_done();
}

static void printbuf(uint8_t buf[FPGA_FLASH_PAGE_SIZE]) {
    for (int i = 0; i < FPGA_FLASH_PAGE_SIZE; ++i) {
        if (i % 16 == 15)
            LOG("%02x\n", buf[i]);
        else
            LOG("%02x ", buf[i]);
    }
}

static void flash_reset(void) {
    cs_select();
    uint8_t cmd = 0x66;
    a(HAL_SPI_Transmit(&FLASH_SPI, &cmd, 1, 1000));
    cs_deselect();
    cs_select();
    cmd = 0x99;
    a(HAL_SPI_Transmit(&FLASH_SPI, &cmd, 1, 1000));
    cs_deselect();
}




struct decomp {
    heatshrink_decoder decoder;
    int remaining;
    uint8_t decompressed_page[FPGA_FLASH_PAGE_SIZE];
    int comp_i;
};

void decomp_init(struct decomp *decomp) {
    heatshrink_decoder_reset(&decomp->decoder);
    decomp->remaining = CROSSPOINT_UNCOMPRESSED_LEN;
    decomp->comp_i = 0;
}

static uint8_t *decomp_next_page(struct decomp *decomp) {
    if (decomp->remaining == 0) while (1);
    const int dec_amount =
        decomp->remaining < FPGA_FLASH_PAGE_SIZE
        ? decomp->remaining
        : FPGA_FLASH_PAGE_SIZE;
    int dec_i = 0;
    while (1) {
        int sz = dec_amount - dec_i;
        size_t polled_sz;
        HSD_poll_res poll_res = heatshrink_decoder_poll(
            &decomp->decoder,
            &decomp->decompressed_page[dec_i],
            sz,
            &polled_sz
        );
        if (poll_res < 0) while (1);
        dec_i += polled_sz;
        if (dec_i == dec_amount) {
            while (dec_i < FPGA_FLASH_PAGE_SIZE) {
                decomp->decompressed_page[dec_i++] = 0xff;
            }
            decomp->remaining -= dec_amount;
            if (decomp->remaining == 0) {
                HSD_finish_res finish_res = heatshrink_decoder_finish(
                    &decomp->decoder
                );
                if (finish_res != HSDR_FINISH_DONE) while (1);
            }
            return decomp->decompressed_page;
        }
        if (poll_res == HSDR_POLL_EMPTY) {
            if (decomp->comp_i >= crosspoint_bin_compressed_len) while (1);
            size_t sunk_sz;
            HSD_sink_res sink_res = heatshrink_decoder_sink(
                &decomp->decoder,
                (uint8_t *) &crosspoint_bin_compressed[decomp->comp_i],
                crosspoint_bin_compressed_len - decomp->comp_i,
                &sunk_sz
            );
            if (sink_res < 0) while (1);
            decomp->comp_i += sunk_sz;
        }
    }
}



static bool validate(struct decomp *decomp) {
    static uint8_t buf[FPGA_FLASH_PAGE_SIZE];
    bool eq = true;
    int sum = 0;
    decomp_init(decomp);
    for (int i=0; ; i++) {
        int addr = i * FPGA_FLASH_PAGE_SIZE;
        if (addr >= CROSSPOINT_UNCOMPRESSED_LEN) {
            break;
        }
        int sz = CROSSPOINT_UNCOMPRESSED_LEN - addr;
        if (sz > FPGA_FLASH_PAGE_SIZE) sz = FPGA_FLASH_PAGE_SIZE;
        flash_read(addr, buf, sz);
        if (eq && memcmp(buf, decomp_next_page(decomp), sz) != 0) {
            eq = false;
        }
        for (int j=0; j<sz; j++) {
            sum += buf[j];
        }
    }
    LOGLN("sum: %d", sum);
    return eq;
}


void app_main(void) {
    // struct fpga_flash ff;

    LOGLN("waiting...");

    HAL_Delay(3000);

    LOGLN("FPGA flash");

    // while (1) {

    //     cs_select();

    //     // uint8_t cmd = 0x9f;
    //     // a(HAL_SPI_Transmit(&FLASH_SPI, &cmd, 1, 1000));
    //     // uint8_t buf[3];
    //     // a(HAL_SPI_Receive(&FLASH_SPI, buf, 3, 1000));

    //     uint8_t buf[4] = {0x9f};
    //     a(HAL_SPI_TransmitReceive(&FLASH_SPI, buf, buf, 4, 1000));

    //     LOGLN("%X %X %X", (unsigned int) buf[1], (unsigned int) buf[2], (unsigned int) buf[3]);

    //     cs_deselect();

    //     HAL_Delay(1000);

    // }

    //////////////////////////////////////////////////////////////////////////////////////////////

    // uint8_t page_buf[FPGA_FLASH_PAGE_SIZE];

    // const uint32_t target_addr = 0;

    // flash_sector_erase(target_addr);
    // flash_read(target_addr, page_buf, FPGA_FLASH_PAGE_SIZE);

    // LOG("After erase:\n");
    // printbuf(page_buf);

    // for (int i = 0; i < FPGA_FLASH_PAGE_SIZE; ++i)
    //     page_buf[i] = i;
    // flash_page_program(target_addr, page_buf);
    // flash_read(target_addr, page_buf, FPGA_FLASH_PAGE_SIZE);

    // LOG("After program:\n");
    // printbuf(page_buf);

    // flash_sector_erase(target_addr);
    // flash_read(target_addr, page_buf, FPGA_FLASH_PAGE_SIZE);

    // LOG("Erase again:\n");
    // printbuf(page_buf);

    //////////////////////////////////////////////////////////////////////////////////////////////

    static struct decomp decomp;

    flash_reset();

    LOGLN("comparing...");
    if (validate(&decomp)) {
        LOGLN("equal");
        return;
    }
    LOGLN("not equal");

    LOGLN("erasing...");
    for (int i=0; i<3; i++) {
        flash_sector_erase_64k(i * FLASH_SECTOR_SIZE_64k);
    }
    LOGLN("erased");

    LOGLN("programming...");
    decomp_init(&decomp);
    for (int i=0; ; i++) {
        int addr = i * FPGA_FLASH_PAGE_SIZE;
        if (addr >= CROSSPOINT_UNCOMPRESSED_LEN) {
            break;
        }
        flash_page_program(addr, decomp_next_page(&decomp));
    }
    LOGLN("done");

    LOGLN("comparing again...");
    if (!validate(&decomp)) {
        LOGLN("invalid");
        while (1);
    }
    LOGLN("success");

    flash_reset();

    return;
}

// void app_main(void) {
//     LOGLN("start base_fpga_flasher");

//     HAL_Delay(5000);
//     LOGLN("GO");

//     uint8_t data[3];

//     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 0);
//     HAL_Delay(2);
//     data[0] = 0x9f;
//     LOGLN("send");
//     HAL_SPI_Transmit(&FLASH_SPI, data, 1, HAL_MAX_DELAY);
//     LOGLN("rx");
//     HAL_SPI_Receive(&FLASH_SPI, data, 3, HAL_MAX_DELAY);
//     LOGLN("done");
//     HAL_Delay(2);
//     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, 1);

//     LOGLN("%x %x %x", (int) data[0], (int) data[1], (int) data[2]);
// }
