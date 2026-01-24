#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "string.h"
#include "macros.h"
#include "rfid.h"
#include "touch.h"
#include "littlefs_records.h"
#include "owi.h"

#define MAX_SUM  200
#define MAX_SKIP 200


QueueHandle_t touchInputIsrEvtQueue = NULL;
// TaskHandle_t kt2ReadTaskHandler = NULL;
static volatile uint32_t lastIsrTime = 0;
static kt1233_decoder_t kt2;
uint32_t keyId;

void IRAM_ATTR comp_rx_isr_handler(void *arg)
{
    uint32_t duration = esp_cpu_get_cycle_count() - lastIsrTime;
    if ((duration) <  50 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)            
        return;
    
    lastIsrTime = esp_cpu_get_cycle_count();
    touch_input_evt evt = {        
        .duration = duration,
        .level = (GPIO.in1.val >> (COMP_RX - 32)) & 1, // .in1 - GPIO 32-39
    };

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(touchInputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

void touch_isr_deferred_task(void *args)
{
    touch_input_evt evt;
    for (;;)
    {
        if (xQueueReceive(touchInputIsrEvtQueue, &evt, portMAX_DELAY))
        {
            kt2_read_edge(evt.level, evt.duration, &kt2);
        }
    }
}

void kt2_read_edge(uint8_t level, uint32_t duration, kt1233_decoder_t* d)
{

    if (d->skipCnt < MAX_SKIP)
    {
        d->skipCnt++;
        return;
    }        

    // gpio_set_level(LED_PIN, evt.level);
    // if (level) GPIO.out_w1ts |= 1UL << LED_PIN;
    // else       GPIO.out_w1tc |= 1UL << LED_PIN;

    if (d->sumCnt < MAX_SUM) // Sum time of logic level
    {
        d->timeSum += duration;
        d->sumCnt++;
        return;
    }

    if (!d->timeAvgCalculated)
    {
        d->timeAvg = d->timeSum / d->sumCnt; // Calculate half period
        d->timeAvgCalculated = true;
    }
    else // Average is calculated
    {
        
        if (level == 1)
            d->timeLow = duration;
        else
            d->timeHigh = duration;

        if ((d->timeHigh > ((d->timeAvg * 1638) >> 10)) && 
            (d->timeHigh < (d->timeAvg * 5)) && !d->syncBitFound) // Sync bit detection (timeHigh > (timeAvg * 1.6))
        {
            d->syncBitFound = true;
            d->startOk = false;
            d->bitCnt = 0;
            d->startWordCnt = 0;
            d->startWord = 0;           
            d->parity = 0;            
            keyId = 0;
            return;            
        }

        if (d->syncBitFound && (level == 1)) // On rising edge determine bit
        {
            uint8_t bit = d->timeLow > d->timeAvg ? 1 : 0;
            if (!d->startOk) // Find and check start word
            {
                d->startWord = d->startWord | (bit << d->startWordCnt);
                d->startWordCnt++;

                if (d->startWordCnt == 3) // Start word found
                {
                    if (d->startWord == 0b010)
                        d->startOk = true;
                    else // Reset on faliure
                    {
                        d->startWordCnt = 0;
                        d->startWord = 0;
                        d->syncBitFound = false;
                        d->bitCnt = 0;
                    }
                }
            }
            else if (d->bitCnt < 32) // Start word ok
            {
                keyId = keyId | (bit << d->bitCnt);
                d->parity = d->parity ^ bit; // Calculate parity
                if ((d->bitCnt & 7) == 7 && d->parity != 0)
                {
                    d->bitCnt = 0;
                    d->syncBitFound = false;
                    d->parity = 0;
                    d->startOk = false;
                }
                d->bitCnt++;
            }
            else if (d->bitCnt == 32)
            {
                // touch_print_t printEvt = {
                //     .evt = 8,                    
                //     .bitCnt = d->bitCnt,
                //     .data = keyId,
                // };
                char str[35];
                printf("Metakom Data %lu, %s\n", keyId, int32_to_char_bin(str, keyId));

                memset(&currentKeyData.value, 0, sizeof(currentKeyData.value)); // Reset key value
                currentKeyData.kt2.id = keyId;
                currentKeyType = KEY_TYPE_KT2;
                ui_event_e event = EVT_KEY_SCAN_DONE;
                xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
                // xQueueSend(printQueue, &printEvt, 0);
                gpio_intr_disable(COMP_RX);                
                
            }
        }
    }
}

void touch_rx_enable()
{
    const char* TAG = "tx_meta";
    // uint8_t printEvt = 4;
    // xQueueSend(printQueue, &printEvt, 0);    
    memset(&kt2, 0, sizeof(kt2)); // Reset decoder
    gpio_set_level(PULLUP_PIN, 0); // Enable pullup
    gpio_set_level(METAKOM_TX, 0);
    esp_err_t err = rmt_disable(touch_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    gpio_intr_enable(COMP_RX);
}

void touch_rx_disable()
{
    gpio_intr_disable(COMP_RX);
    gpio_set_level(PULLUP_PIN, 1); // Disable pullip PNP
}

void transmit_metakom_k2()
{
    const char* TAG = "tx_meta";
    esp_err_t err = rmt_disable(touch_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    err = rmt_enable(touch_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);

    static rmt_symbol_word_t pattern[36];
    uint32_t period = 200;
    uint8_t startWord = 2;
    uint32_t data = keyId;
    rmt_symbol_word_t bit_0 = {{period * 0.4, 0, period * 0.6, 1}};
    rmt_symbol_word_t bit_1 = {{period * 0.6, 0, period * 0.4, 1}};

    pattern[0] = (rmt_symbol_word_t){{period / 2, 1, period / 2, 1}}; // Sync bit and start word
    for (uint8_t i = 1; i < 4; i++)
    {
        pattern[i] = startWord & 1 ? bit_1 : bit_0;
        startWord >>= 1;
    }

    for (uint8_t i = 4; i < 36; i++)
    {
        pattern[i] = data & 1 ? bit_1 : bit_0;
        data >>= 1;
    }

    ESP_ERROR_CHECK(rmt_transmit(touch_tx_ch, copy_enc, pattern, sizeof(pattern), &touch_tx_config));    
}

void touch_read_task(void* args)
{
    while (1) 
    {
        gpio_intr_disable(COMP_RX);

        if (owi_read_rom(currentKeyData.bytes))
        {
            currentKeyType = KEY_TYPE_IBUTTON;
            ui_event_e event = EVT_KEY_SCAN_DONE;
            xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
        }
        else
        {
            gpio_intr_enable(COMP_RX);
            xQueueReset(touchInputIsrEvtQueue);

        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}