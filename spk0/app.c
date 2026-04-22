#include "app.h"

#ifdef DBG_UART
char logln_buf[256];
#endif

#include "../mcp_module_stm32/mcp_module_stm32.h"

extern I2S_HandleTypeDef hi2s1;
extern DAC_HandleTypeDef hdac1;

extern TIM_HandleTypeDef htim16;

volatile uint8_t volume_shift;

static uint16_t i2s_word;

static const mcp_module_stm32_pin_t clk_dat_pins[2] = {
    {GPIOB, GPIO_PIN_6},
    {GPIOB, GPIO_PIN_7}
};

static const char main_4th_body[] = R"(
here 1 allot constant buf

mcpd_driver_connect
0= s" mcpd_driver_connect failure" assert_msg
constant con

con buf 1 mcpd_read
buf c@ constant socketno

con MCP_PINS_PERIPH_TYPE_I2S MCP_PINS_DRIVER_TYPE_I2S_RAW mcpd_resource_acquire
dup 0 >= s" i2s acquire fail" assert_msg
constant resource_id

con resource_id MCP_PINS_PIN_I2S_BCLK socketno 1 mcpd_resource_route
0= s" BCLK route fail" assert_msg

con resource_id MCP_PINS_PIN_I2S_WS socketno 0 mcpd_resource_route
0= s" WS route fail" assert_msg

con resource_id MCP_PINS_PIN_I2S_DATA socketno 2 mcpd_resource_route
0= s" DATA route fail" assert_msg

begin 30000 ms again
)";

static const mcp_module_static_file_table_entry_t static_file_table[] = {
    {"info", "{\"name\":\"spk0\"}\n", sizeof("{\"name\":\"spk0\"}\n") - 1},
    {"main.4th", main_4th_body, sizeof(main_4th_body) - 1}
};

__attribute__ ((optimize ("O2")))
void handle_framing_error(void)
{
    __HAL_I2S_DISABLE(&hi2s1);

    uint16_t t_start = __HAL_TIM_GET_COUNTER(&MICROSECOND_TIMER);

    while(1) {
        // if WS is high the frame is aligned now, so re-enable i2s and return
        if((GPIOB->IDR & GPIO_PIN_0) != 0) {
            break;
        }

        // if it's been more than 150us give up waiting
        // for WS high. The lowest sample rate 8000 samples/s
        // would see WS high by now. 8000Hz^-1 == 125us
        uint16_t t_now = __HAL_TIM_GET_COUNTER(&MICROSECOND_TIMER);
        uint16_t t_diff = t_now - t_start;
        if(t_diff > 150) {
            break;
        }
    }

    __HAL_I2S_ENABLE(&hi2s1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    static bool last3 = true;
    static bool last4 = true;

    uint16_t portb = GPIOB->IDR;
    bool p3 = portb & GPIO_PIN_3;
    bool p4 = portb & GPIO_PIN_4;

    if(last3 && !p3) {
        uint8_t vsh = volume_shift;
        if(vsh != 32) {
            if(vsh == 6) {
                volume_shift = 32;
            }
            else {
                volume_shift = vsh + 1;
            }
        }
    }
    last3 = p3;

    if(last4 && !p4) {
        uint8_t vsh = volume_shift;
        if(vsh != 0) {
            if(vsh == 32) {
                volume_shift = 6;
            }
            else {
                volume_shift = vsh - 1;
            }
        }
    }
    last4 = p4;
}

static void driver_protocol_cb(mcp_module_driver_handle_t * hdl, void * driver_protocol_ctx)
{
    uint8_t whereami = mcp_module_driver_whereami(hdl);
    mcp_module_driver_write(hdl, &whereami, 1);
}

void app_main(void) {
    LOGLN("start spk0");

    HAL_TIM_Base_Start(&MICROSECOND_TIMER);


    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);

    HAL_I2S_Receive_IT(&hi2s1, &i2s_word, 1);

    HAL_TIM_Base_Start_IT(&htim16);

    mcp_module_stm32_run(
        clk_dat_pins,
        &MICROSECOND_TIMER,
        static_file_table,
        sizeof(static_file_table) / sizeof(mcp_module_static_file_table_entry_t),
        NULL,
        NULL,
        NULL,
        driver_protocol_cb
    );
}
