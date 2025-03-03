#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_bitbang/mcp_bitbang_client.h"

#include <stddef.h>
#include <inttypes.h>
#include <assert.h>

typedef struct {GPIO_TypeDef *port; uint16_t pin;} pin_t;

static const pin_t pins[2] = {
    {GPIOA, GPIO_PIN_8},
    {GPIOA, GPIO_PIN_12}
};

static bool read_cb(void * caller_ctx, mbb_cli_pin_t pinno)
{
    const pin_t * pin = &pins[pinno];
    return HAL_GPIO_ReadPin(pin->port, pin->pin);
}

static void write_cb(void * caller_ctx, mbb_cli_pin_t pinno, bool val)
{
    const pin_t * pin = &pins[pinno];
    HAL_GPIO_WritePin(pin->port, pin->pin, val);
}

static void run_transfer(mbb_cli_t * cli)
{
    mbb_cli_status_t status;
    while (MBB_CLI_STATUS_DONE != (status = mbb_cli_continue_byte_transfer(cli))) {
        switch (status) {
            case MBB_CLI_STATUS_DO_DELAY:
                HAL_Delay(2);
                break;
            case MBB_CLI_STATUS_DO_DELAY_AND_WAIT_CLK_PIN_HIGH:
                HAL_Delay(2);
                /* fallthrough */
            case MBB_CLI_STATUS_WAIT_CLK_PIN_HIGH:
                while(!read_cb(NULL, MBB_CLI_PIN_CLK));
                break;
            default:
                assert(0);
        }
    }
}

static uint8_t do_read(mbb_cli_t * cli)
{
    mbb_cli_start_byte_transfer(cli, MBB_CLI_BYTE_TRANSFER_READ);
    run_transfer(cli);
    return mbb_cli_get_read_byte(cli);
}

static void do_write(mbb_cli_t * cli, uint8_t data)
{
    mbb_cli_start_byte_transfer(cli, MBB_CLI_BYTE_TRANSFER_WRITE(data));
    run_transfer(cli);
}

void app_main(void) {
    LOGLN("start dbg0");

    HAL_Delay(100); // give the base time to start up

    write_cb(NULL, MBB_CLI_PIN_CLK, true);
    write_cb(NULL, MBB_CLI_PIN_DAT, true);

    while(!read_cb(NULL, MBB_CLI_PIN_CLK) || !read_cb(NULL, MBB_CLI_PIN_DAT));
    HAL_Delay(2);
    write_cb(NULL, MBB_CLI_PIN_CLK, false);
    HAL_Delay(2);
    write_cb(NULL, MBB_CLI_PIN_CLK, true);
    HAL_Delay(2);


    static mbb_cli_t cli;
    mbb_cli_init(&cli, read_cb, write_cb, NULL);

    do_write(&cli, 255);
    uint8_t token = do_read(&cli);
    LOGLN("token: %d", (int) token);

    do_write(&cli, 5); // whereami
    uint8_t where = do_read(&cli);
    LOGLN("where: %d", (int) where);

    do_write(&cli, 0); // write
    do_write(&cli, 4); // "dbg0" is 4 bytes
    do_write(&cli, token); // send to self
    uint8_t free_space = do_read(&cli);
    LOGLN("free space: %d", (int) free_space);
    assert(free_space >= 4);
    do_write(&cli, 'd');
    do_write(&cli, 'b');
    do_write(&cli, 'g');
    do_write(&cli, '0');

    do_write(&cli, 1); // read
    do_write(&cli, 4); // "dbg0" is 4 bytes
    do_write(&cli, token); // recv from self
    uint8_t readable = do_read(&cli);
    LOGLN("amount readable: %d", (int) readable);
    assert(readable == 4);
    assert(do_read(&cli) == 'd');
    assert(do_read(&cli) == 'b');
    assert(do_read(&cli) == 'g');
    assert(do_read(&cli) == '0');
    LOGLN("'dbg0' received");

    while (1) {
        for(int i = 0; i < 4; i++) {
            uint8_t pin = i << 1;
            do_write(&cli, 4); // crosspoint
            do_write(&cli, 255); // constant 1
            do_write(&cli, where); // set own output
            do_write(&cli, pin | 1); // enable
            HAL_Delay(200);
            do_write(&cli, 4); // crosspoint
            do_write(&cli, 255); // constant 1
            do_write(&cli, where); // set own output
            do_write(&cli, pin); // disable
        }
    }

    // unsigned x = 0;
    // while(1) {
    //     LOGLN("===%u???", x++);
    //     HAL_Delay(100);
    // }
}
