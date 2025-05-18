#include "macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h> // For memset
#include "esp_intr_alloc.h"
#include "nvs.h"
#
#include "ssd1306.h"
// #include "font8x8_basic.h"

#include "rfid.h"

#define ESP_INTR_FLAG_DEFAULT   0
#define READ_MODE               0
#define TRANSMIT_MODE           1

#define RMT_SIZE 64

static const char *TAG = "ISR_offload";
static const char *TAG2 = "button_offload";
// gptimer_handle_t signalTimer = NULL;
rmt_channel_handle_t tx_chan = NULL;
rmt_encoder_handle_t copy_enc;
rmt_transmit_config_t trans_config  = {
        .loop_count = -1,
        .flags.eot_level = false,
        .flags.queue_nonblocking = true,
    };
rmt_symbol_word_t pulse_pattern[RMT_SIZE];
SemaphoreHandle_t modeSwitchSem;
SSD1306_t dev;
uint64_t tag1 = 0xff8e2001a5761700, tag2 = 0x900b7c28d;


void raw_tag_to_rmt(rmt_symbol_word_t *rmtArr, uint64_t rawTag);

void rfid_deferred_task(void *arg)
{
    rfid_read_event_t evt;
    uint64_t last_tag = 0;
    char str[70];    
    for (;;)
    {
        if (xQueueReceive(inputIsrEvtQueue, &evt, portMAX_DELAY))
        {    
            {
                // ESP_LOGI(TAG, "Level: %d, Time: %lu, idx: %ld, buff: %s", evt.level, evt.ms, evt.idx, int_to_char_bin(str,evt.buf));
                uint64_t long_tag = 0;
                for (uint8_t i = 0; i < 5; i++)
                    long_tag |= ((uint64_t)evt.tag[i] << (8 * i));
                if (last_tag != long_tag)
                {
                    last_tag = long_tag;
                    uint32_t tick1 = esp_cpu_get_cycle_count();

                    sprintf(str, "0x%010" PRIX64, long_tag);
                    ssd1306_display_text(&dev, 0, str, 12, 0);
                    // ESP_LOGI(TAG, "tag: %#llx, ticks: %ld", long_tag, esp_cpu_get_cycle_count() - tick1);
                }                

            }
        }
    }
}

void button_interrupt_handler(void *arg)
{
    static uint32_t last_tick = 0;
    gpio_intr_disable(BUTTON_PIN);
    uint32_t current_tick = esp_cpu_get_cycle_count();
    
    if ((current_tick - last_tick)  < 250 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000 )
    {
        gpio_intr_enable(BUTTON_PIN);
        return;
    }    
    last_tick = current_tick;

    gpio_intr_enable(BUTTON_PIN);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(modeSwitchSem, &woken);
    if (woken == pdTRUE) {
        // If giving the semaphore woke a higher‐priority task, yield now
        portYIELD_FROM_ISR();
    }
    
}

static void mode_switch_task(void *params)
{
    uint8_t mode = TRANSMIT_MODE;

    for (;;)
    {
        // Wait indefinitely for a button press event
        if (xSemaphoreTake(modeSwitchSem, portMAX_DELAY) == pdTRUE)
        {
            // Toggle mode
            mode = (mode == READ_MODE) ? TRANSMIT_MODE : READ_MODE;
        }

        if (mode == READ_MODE)
        {
            // Disable RMT TX
            // trans_config.loop_count = -1;
            // ESP_LOGI(TAG2, "trying set loop to 1");
            // ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_chan, portMAX_DELAY));
            esp_err_t err = rmt_disable(tx_chan);
            if (err != ESP_ERR_INVALID_STATE && err != ESP_OK )
                ESP_LOGE(TAG2, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
            // trans_config.loop_count = -1;
            ESP_ERROR_CHECK(rmt_enable(tx_chan));
            for (uint8_t i = 0; i < 64; i++)
            {       
                pulse_pattern[i].duration0  = 4;
                pulse_pattern[i].duration1  = 4;
                pulse_pattern[i].level0     = 1;
                pulse_pattern[i].level1     = 0;        
            } 
            ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, pulse_pattern, sizeof(pulse_pattern), &trans_config));
            ESP_LOGI(TAG2, "rmt tx carrier");
            // Enable coil VCC
            gpio_set_level(COIL_VCC_PIN, 1);
            // Enable coil carrier signal LEDC
            // ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 512));
            // ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
            // Enable GPIO input signal interrupt
            ESP_ERROR_CHECK(gpio_intr_enable(INPUT_SIGNAL_PIN));

            gpio_set_level(LED_PIN, 1);
        }
        else if (mode == TRANSMIT_MODE)
        {
            // Disable GPIO input signal interrupt
            ESP_ERROR_CHECK(gpio_intr_disable(INPUT_SIGNAL_PIN));
            // Disable coil VCC
            gpio_set_level(COIL_VCC_PIN, 0);
            // Disable coil carrier signal LEDC
            // ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0));
            // ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
            // Enable RMT and start tx
            // ESP_LOGI(TAG2, "trying set loop to -1");
            // trans_config.loop_count = -1;
            
            raw_tag_to_rmt(pulse_pattern, tag1);
            // ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_chan, portMAX_DELAY));
                        esp_err_t err = rmt_disable(tx_chan);
            if (err != ESP_ERR_INVALID_STATE && err != ESP_OK )
                ESP_LOGE(TAG2, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
            ESP_ERROR_CHECK(rmt_enable(tx_chan));
            // trans_config.loop_count = -1;
            ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, pulse_pattern, sizeof(pulse_pattern), &trans_config));
            ESP_LOGI(TAG2, "rmt tx tag");
            gpio_set_level(LED_PIN, 0);
        }
    }
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

#define tag "SSD1306"

void app_main(void)
{
    
	int center, top, bottom;

    #if CONFIG_I2C_INTERFACE
	ESP_LOGI(tag, "INTERFACE is i2c");
	ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE

#if CONFIG_FLIP
	dev._flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
#endif
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);  


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
    // gptimer_config_t timer_config = {
    //     .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    //     .direction = GPTIMER_COUNT_UP,
    //     .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    // };
    // ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &signalTimer));
    // // Configure the timer alarm to trigger 
    //  gptimer_alarm_config_t alarm_config = {
    //     .alarm_count = 256,  // Set the alarm value to trigger interrupt
    //     .reload_count = 0,                 // No need to reload the timer after alarm trigger
    //     .flags.auto_reload_on_alarm = true, // The timer counts up

    // };
    // gptimer_event_callbacks_t cbs ={
    //     .on_alarm = on_timer_alarm,
    // };
    // ESP_ERROR_CHECK(gptimer_set_alarm_action(signalTimer, &alarm_config));
    // ESP_ERROR_CHECK(gptimer_register_event_callbacks(signalTimer, &cbs, NULL));
    // ESP_ERROR_CHECK(gptimer_enable(signalTimer));    
    // ESP_ERROR_CHECK(gptimer_start(signalTimer));

    // Create the queue. 
    inputIsrEvtQueue = xQueueCreate(200, sizeof(rfid_read_event_t));
    if (inputIsrEvtQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
//-----------------------------------------------------------------------------
// Signal input GPIO interrupt setup    
    gpio_config_t inputSingal_config = {
        .intr_type = GPIO_INTR_ANYEDGE, 
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << INPUT_SIGNAL_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,     
        .pull_down_en = GPIO_PULLDOWN_DISABLE         
    };
    ESP_ERROR_CHECK(gpio_config(&inputSingal_config));
    // Install GPIO ISR service and add the handler for our pin.
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT_SIGNAL_PIN, rfid_read_isr_handler, (void *)INPUT_SIGNAL_PIN));

//Button GPIO interrupt setup
    gpio_config_t buttonState_config = {
        .intr_type = GPIO_INTR_POSEDGE, 
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,     
        .pull_down_en = GPIO_PULLDOWN_DISABLE         
    };
    ESP_ERROR_CHECK(gpio_config(&buttonState_config));
    // Install GPIO ISR service and add the handler for our pin.    
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, button_interrupt_handler, (void *)BUTTON_PIN));
//-----------------------------------------------------------------------------
// LED pin setup
    gpio_config_t led_io = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_2),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pull-up if needed
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&led_io));
//-----------------------------------------------------------------------------
//RMT TX tag transmit
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = COIL_OUTPUT_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1 MHz resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
    };
    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_chan));
    rmt_copy_encoder_config_t copy_cfg = {};    
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &copy_enc));
    // ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, pulse_pattern, sizeof(pulse_pattern), &trans_config));
    // ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, &carrier_pattern, sizeof(carrier_pattern), &trans_config));
    //-----------------------------------------------------------------------------
    // Create a task to process the deferred events.
    xTaskCreate(rfid_deferred_task, "rfid_deferred_task", 3048, NULL, 1, NULL);

    //-----------------------------------------------------------------------------
    // Create semaphore for mode switching
    modeSwitchSem = xSemaphoreCreateBinary();
    configASSERT(modeSwitchSem != NULL);
    // Create the worker task
    xTaskCreate(mode_switch_task, "mode_switch", 2048, 0, 10, NULL);
    // ESP_ERROR_CHECK(rmt_disable(tx_chan));
    xSemaphoreGive(modeSwitchSem);

}