#include "macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "driver/rmt_tx.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h> // For memset
#include "esp_intr_alloc.h"
#include "rfid.h"

#define ESP_INTR_FLAG_DEFAULT 0

#define RMT_SIZE 64
rmt_symbol_word_t pulse_pattern[RMT_SIZE];

static const char *TAG = "ISR_offload";
gptimer_handle_t signalTimer = NULL;
uint64_t tag1 = 0xff8e2001a5761700, tag2 = 0x900b7c28d;


void rfid_deferred_task(void *arg)
{
    rfid_read_event_t evt;
    // char str[65];
    
    for (;;)
    {
        if (xQueueReceive(inputIsrEvtQueue, &evt, portMAX_DELAY))
        {

            // if (evt.tag[3] != 0)       
            {
                // ESP_LOGI(TAG, "Level: %d, Time: %lu, idx: %ld, buff: %s", evt.level, evt.ms, evt.idx, int_to_char_bin(str,evt.buf));
                uint64_t long_tag = 0;
                for (uint8_t i = 0; i < 5; i++)
                    long_tag |= ((uint64_t)evt.tag[i] << (8 * i));
                // long_tag |= (uint64_t)evt.tag[0];
                // long_tag |= (uint64_t)evt.tag[1] << 8;
                // long_tag |= (uint64_t)evt.tag[2] << 16;
                // long_tag |= (uint64_t)evt.tag[3] << 24;
                // long_tag |= (uint64_t)evt.tag[4] << 32;
                
                ESP_LOGI(TAG, "tag: %#llx", long_tag);
                // ESP_LOGI(TAG, "tag: %#x %x %x %x %x", evt.tag[4],evt.tag[3],evt.tag[2],evt.tag[1],evt.tag[0]);
            }

        }
    }
}

bool IRAM_ATTR on_timer_alarm(gptimer_handle_t t, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static uint8_t bit = 0, clk = 0;

    uint8_t level = ((tag1 >> (63-bit)) & 1) ^ clk;
    if (clk > 0)
        bit = (bit + 1) % 64;
    clk = !clk;
    
    gpio_set_level(COIL_OUTPUT_PIN, level);
    gpio_set_level(LED_PIN, level);
    return false;
}

void raw_tag_to_rmt(rmt_symbol_word_t *rmtArr, uint64_t rawTag)
{
    for (uint8_t i = 0; i < 64; i++)
    {
        uint8_t bit = rawTag >> (63 - i) & 1;
        if (bit > 0)
        {
            rmtArr[i].level0 = 0;
            rmtArr[i].level1 = 1;
        }
        else
        {
            rmtArr[i].level0 = 1;
            rmtArr[i].level1 = 0;
        }
        rmtArr[i].duration0 = 256;
        rmtArr[i].duration1 = 256;
    }
}

void app_main(void)
{
    gpio_set_direction(COIL_VCC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(COIL_VCC_PIN, 0);  

    gpio_set_direction(COIL_OUTPUT_PIN, GPIO_MODE_OUTPUT);
 
//-----------------------------------------------------------------------------
// LEDC Setup and Start
// Coil signal source

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_2_BIT, // 1-bit resolution for 50% duty
        .freq_hz          = 125000,           // 125 kHz frequency
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = COIL_OUTPUT_PIN,       // Replace with your desired GPIO
        .duty           = 2,                 // 50% duty cycle for 1-bit resolution
        .hpoint         = 0
    };
    // ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    // ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 512));
//-----------------------------------------------------------------------------
// General Purpouse Timer Setup
// Timer for impulse time counting
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &signalTimer));
    // Configure the timer alarm to trigger 
     gptimer_alarm_config_t alarm_config = {
        .alarm_count = 256,  // Set the alarm value to trigger interrupt
        .reload_count = 0,                 // No need to reload the timer after alarm trigger
        .flags.auto_reload_on_alarm = true, // The timer counts up

    };
    gptimer_event_callbacks_t cbs ={
        .on_alarm = on_timer_alarm,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(signalTimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(signalTimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(signalTimer));    
    ESP_ERROR_CHECK(gptimer_start(signalTimer));

    // Create the queue. 
    inputIsrEvtQueue = xQueueCreate(100, sizeof(rfid_read_event_t));
    if (inputIsrEvtQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
//-----------------------------------------------------------------------------
// GPIO Interrupt Setup

    // Configure the GPIO pin for input with the desired pull configuration.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE, // Interrupt on any state change
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << INPUT_SIGNAL_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable pull-up if needed
        .pull_down_en = GPIO_PULLDOWN_DISABLE 
        
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Install GPIO ISR service and add the handler for our pin.
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT_SIGNAL_PIN, rfid_read_isr_handler, (void *)INPUT_SIGNAL_PIN));
//-----------------------------------------------------------------------------
// LED pin setup
    gpio_config_t led_io = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_2),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pull-up if needed
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&led_io));

    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = 19,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1 MHz resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
    };
    rmt_channel_handle_t tx_chan = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_chan));
    ESP_ERROR_CHECK(rmt_enable(tx_chan));
    rmt_transmit_config_t trans_config = {
        .loop_count = -1,
        .flags.eot_level = false,
        .flags.queue_nonblocking = true,
    };
    raw_tag_to_rmt(pulse_pattern, tag1);
    rmt_copy_encoder_config_t copy_cfg = { /* no fields yet */ };
    rmt_encoder_handle_t copy_enc;
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &copy_enc));

    ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, pulse_pattern, sizeof(pulse_pattern), &trans_config));
    //-----------------------------------------------------------------------------
    // Create a task to process the deferred events.

    xTaskCreate(rfid_deferred_task, "rfid_deferred_task", 2048, NULL, 1, NULL);
}