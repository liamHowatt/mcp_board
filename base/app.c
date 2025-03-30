#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_bitbang/mcp_bitbang_server.h"
#include "../mcp_modnet/mcp_modnet_server.h"

// #include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#define CRESET_B GPIOF, GPIO_PIN_0
#define CDONE    GPIOC, GPIO_PIN_15

#define SOCKET_COUNT 12
#define BUF_SIZE     192

typedef struct {
    mmn_srv_t srv;
    mmn_srv_member_t memb[SOCKET_COUNT];

    volatile mmn_srv_crosspoint_command_t xpoint_command;
    volatile bool xpoint_is_transferring;

    // volatile bool something_happened;
} stm32_srv_t;

typedef struct {
    volatile mbb_srv_transfer_t trn;
    volatile bool is_transferring;
    volatile uint8_t index;
    volatile bool flip;
} ctx_t;

typedef struct {GPIO_TypeDef *port; uint16_t pin;} pin_t;

static const pin_t pins[SOCKET_COUNT][2] = {
    {
        {GPIOC, GPIO_PIN_7}, // FAHX
        {GPIOC, GPIO_PIN_6} // FAHY
    },{
        {GPIOA, GPIO_PIN_11}, // FBHX
        {GPIOA, GPIO_PIN_10} // FBHY
    },{
        {GPIOB, GPIO_PIN_6}, // FCHX
        {GPIOB, GPIO_PIN_5} // FCHY
    },{
        {GPIOD, GPIO_PIN_2}, // FDHX
        {GPIOD, GPIO_PIN_3} // FDHY
    },{
        {GPIOB, GPIO_PIN_8}, // FEHX
        {GPIOB, GPIO_PIN_7} // FEHY
    },{
        {GPIOB, GPIO_PIN_1}, // FFHX
        {GPIOB, GPIO_PIN_0} // FFHY
    },{
        {GPIOA, GPIO_PIN_12}, // BAHX
        {GPIOA, GPIO_PIN_15} // BAHY
    },{
        {GPIOB, GPIO_PIN_15}, // BBHX
        {GPIOA, GPIO_PIN_9} // BBHY
    },{
        {GPIOD, GPIO_PIN_0}, // BCHX
        {GPIOD, GPIO_PIN_1} // BCHY
    },{
        {GPIOB, GPIO_PIN_3}, // BDHX
        {GPIOB, GPIO_PIN_4} // BDHY
    },{
        {GPIOB, GPIO_PIN_2}, // BEHX
        {GPIOB, GPIO_PIN_10} // BEHY
    },{
        {GPIOB, GPIO_PIN_9}, // BFHX
        {GPIOC, GPIO_PIN_13}  // BFHY
    }
};

static const pin_t xpoint_pins[3] = {
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_11}
};

static stm32_srv_t g_srv;

static uint32_t get_tick_ms(mmn_srv_t * srv)
{
    return HAL_GetTick();
}

static void set_trn(void * vctx, mbb_srv_transfer_t trn)
{
    ctx_t * ctx = vctx;
    ctx->trn = trn;
    __DMB();
    ctx->is_transferring = true;
}

static bool get_trn(void * vctx, mbb_srv_transfer_t * trn_dst)
{
    ctx_t * ctx = vctx;
    if(ctx->is_transferring) {
        *trn_dst = ctx->trn;
        return true;
    }
    return false;
}

static void set_trn_done(void * vctx, uint8_t read_byte)
{
    ctx_t * ctx = vctx;
    ctx->trn = read_byte;
    __DMB();
    ctx->is_transferring = false;
}

static bool get_trn_done(void * vctx, uint8_t * read_byte_dst)
{
    ctx_t * ctx = vctx;
    if(!ctx->is_transferring) {
        *read_byte_dst = ctx->trn;
        return true;
    }
    return false;
}

static bool pin_read(void * vctx, mbb_srv_pin_t pinno)
{
    ctx_t * ctx = vctx;
    const pin_t * pin = &pins[ctx->index][pinno];
    return HAL_GPIO_ReadPin(pin->port, pin->pin);
}

static void pin_write(void * vctx, mbb_srv_pin_t pinno, bool value)
{
    ctx_t * ctx = vctx;
    const pin_t * pin = &pins[ctx->index][pinno];
    HAL_GPIO_WritePin(pin->port, pin->pin, value);
}

static void set_xpoint(mmn_srv_t * vsrv, mmn_srv_crosspoint_command_t command)
{
    stm32_srv_t * srv = (stm32_srv_t *) vsrv;
    srv->xpoint_command = command;
    __DMB();
    srv->xpoint_is_transferring = true;
}
static bool get_xpoint(mmn_srv_t * vsrv, mmn_srv_crosspoint_command_t * command_dst)
{
    stm32_srv_t * srv = (stm32_srv_t *) vsrv;
    if(srv->xpoint_is_transferring) {
        *command_dst = srv->xpoint_command;
        return true;
    }
    return false;
}
static void set_xpoint_done(mmn_srv_t * vsrv)
{
    stm32_srv_t * srv = (stm32_srv_t *) vsrv;
    srv->xpoint_is_transferring = false;
}
static bool get_xpoint_done(mmn_srv_t * vsrv)
{
    stm32_srv_t * srv = (stm32_srv_t *) vsrv;
    return !srv->xpoint_is_transferring;
}
static void xpoint_pin_write(mmn_srv_t * vsrv, mmn_srv_xpoint_pin_t pinno, bool en)
{
    const pin_t * pin = &xpoint_pins[pinno];
    HAL_GPIO_WritePin(pin->port, pin->pin, en);
}

static void set_flip(void * vctx, bool flip)
{
    ctx_t * ctx = vctx;
    ctx->flip = flip;
    __DMB();
}
static uint32_t fpga_decode_pinno(void * vctx, uint32_t pinno)
{
    static const bool pins_out_of_order[SOCKET_COUNT] = {
        0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1
    };

    ctx_t * ctx = vctx;
    bool flip = ctx->flip;
    uint8_t index = ctx->index;

    bool is_out_of_order = pins_out_of_order[index];

    bool reversed = flip ^ is_out_of_order;

    if(reversed) {
        return 3 - pinno;
    }
    return pinno;
}

static const mmn_srv_cbs_t cbs = {
    .get_tick_ms = get_tick_ms,
    .set_trn = set_trn,
    .get_trn = get_trn,
    .set_trn_done = set_trn_done,
    .get_trn_done = get_trn_done,
    .pin_cbs = {
        .read = pin_read,
        .write = pin_write
    },
    .set_xpoint = set_xpoint,
    .get_xpoint = get_xpoint,
    .set_xpoint_done = set_xpoint_done,
    .get_xpoint_done = get_xpoint_done,
    .xpoint_pin_write = xpoint_pin_write,
    .set_flip = set_flip,
    .fpga_decode_pinno = fpga_decode_pinno
};

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    mmn_srv_timer_isr_handler(&g_srv.srv);
    // g_srv.something_happened = true;
}

void app_main(void) {
    LOGLN("start base");

    HAL_GPIO_WritePin(CRESET_B, 1);

    static uint8_t aux_memory[MMN_SRV_AUX_MEMORY_SIZE(SOCKET_COUNT, BUF_SIZE)];
    static ctx_t ctxs[SOCKET_COUNT];
    mmn_srv_init(&g_srv.srv, SOCKET_COUNT, 125, BUF_SIZE, aux_memory, &cbs);
    for(uint8_t i = 0; i < SOCKET_COUNT; i++) {
        ctxs[i].index = i;
        ctxs[i].flip = false;
        mmn_srv_member_init(&g_srv.srv, &g_srv.memb[i], i, &ctxs[i]);
    }

    while(!HAL_GPIO_ReadPin(CDONE));
    HAL_Delay(2);

    LOGLN("starting timer");
    __DMB();
    HAL_TIM_Base_Start_IT(&INTERRUPTER_TIMER);

    while(1) {
        mmn_srv_main_loop_handler(&g_srv.srv);
        // while (1) {
        //     __disable_irq();
        //     if(!g_srv.something_happened) {
        //         __WFI();
        //     } else {
        //         g_srv.something_happened = false;
        //         __enable_irq();
        //         break;
        //     }
        //     __enable_irq();
        // }
    }

    // while(1) {
    //     HAL_Delay(100);
    //     uint32_t tick = HAL_GetTick();
    //     uint32_t sec = tick / 1000;
    //     LOGLN("%"PRIu32" %"PRIu32, sec, tick);
    // }
}
