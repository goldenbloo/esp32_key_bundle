#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "macros.h"
#include "rfid.h"
#include "touch.h"

#define MAX_SUM  1000
#define MAX_SKIP 2000


QueueHandle_t touchInputIsrEvtQueue = NULL;
TaskHandle_t kt2ReadTaskHandler = NULL;
static bool timeAvgCalculated = false, syncBitFound = false, startOk = false;
static volatile uint32_t lastIsrTime = 0, lastTime = 0;
static uint32_t sumCnt = 0, timeHigh = 0, timeLow = 0, timeAvg = 0, timeSum = 0, skipCnt = 0;
static uint8_t bitCnt = 0, startWordCnt = 0, startWord = 0, parity = 0;
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

void read_metakom_kt2()
{
    const char* TAG = "tx_meta";

    uint8_t printEvt = 4;
    xQueueSend(printQueue, &printEvt, 0);

    timeHigh = timeLow = timeAvg = sumCnt = timeSum = 0;
    syncBitFound = false;
    timeAvgCalculated = false;
    lastTime = esp_cpu_get_cycle_count();
    skipCnt = 0;

    gpio_set_level(PULLUP_PIN, 0); // Enable pullup
    gpio_set_level(METAKOM_TX, 0);
    esp_err_t err = rmt_disable(touch_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    
    if (kt2ReadTaskHandler == NULL)
        xTaskCreate(touch_isr_deferred_task, "touch_isr_deferred_task", 2048, NULL, 4, &kt2ReadTaskHandler);
    gpio_set_intr_type(COMP_RX, GPIO_INTR_ANYEDGE);
}

void touch_isr_deferred_task(void *args)
{
    touch_input_evt evt;
    for (;;)
    {
        if (xQueueReceive(touchInputIsrEvtQueue, &evt, portMAX_DELAY))
        {
            kt2_read_edge(evt.level, evt.duration);
        }
    }
}

void kt2_read_edge(uint8_t level, uint32_t duration)
{

    if (skipCnt < MAX_SKIP)
    {
        skipCnt++;
        return;
    }        

    // gpio_set_level(LED_PIN, evt.level);
    if (level) GPIO.out_w1ts |= 1UL << LED_PIN;
    else       GPIO.out_w1tc |= 1UL << LED_PIN;

    if (sumCnt < MAX_SUM) // Sum time of logic level
    {
        timeSum += duration;
        sumCnt++;
        return;
    }

    if (!timeAvgCalculated)
    {
        timeAvg = timeSum / sumCnt; // Calculate half period
        timeAvgCalculated = true;
    }
    else // Average is calculated
    {
        if (level == 1)
            timeLow = duration;
        else
            timeHigh = duration;

        if ((timeHigh > ((timeAvg * 1638) >> 10)) && (timeHigh < (timeAvg * 5)) && !syncBitFound) // Sync bit detection (timeHigh > (timeAvg * 1.6))
        {
            syncBitFound = true;
            bitCnt = 0;
            startWordCnt = 0;
            startWord = 0;
            keyId = 0;
            parity = 0;
            startOk = false;
            return;            
        }

        if (syncBitFound && (level == 1)) // On rising edge determine bit
        {
            uint8_t bit = timeLow > timeAvg ? 1 : 0;

            if (!startOk) // Find and check start word
            {
                startWord = startWord | (bit << startWordCnt);
                startWordCnt++;

                if (startWordCnt == 3) // Start word found
                {
                    if (startWord == 0b010)
                        startOk = true;
                    else // Reset on faliure
                    {
                        startWordCnt = 0;
                        startWord = 0;
                        syncBitFound = false;
                        bitCnt = 0;
                    }
                }
            }
            else if (bitCnt < 32) // Start word ok
            {
                keyId = keyId | (bit << bitCnt);
                parity = parity ^ bit; // Calculate parity
                if ((bitCnt & 7) == 7 && parity != 0)
                {
                    bitCnt = 0;
                    syncBitFound = false;
                    parity = 0;
                    startOk = false;
                }
                bitCnt++;
            }
            else if (bitCnt == 32)
            {
                touch_print_t printEvt = {
                    .evt = 8,
                    .tick = lastTime,
                    .bitCnt = bitCnt,
                    .data = keyId,
                };
                xQueueSend(printQueue, &printEvt, 0);
                gpio_set_intr_type(COMP_RX, GPIO_INTR_DISABLE);
                kt2ReadTaskHandler = NULL;
                vTaskDelete(NULL);
            }
        }
    }
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
    uint32_t halfPeriod = 100;
    uint8_t startWord = 2;
    uint32_t data = keyId;
    rmt_symbol_word_t bit_0 = {{halfPeriod * 0.8, 0, halfPeriod * 1.2, 1}};
    rmt_symbol_word_t bit_1 = {{halfPeriod * 1.2, 0, halfPeriod * 0.8, 1}};

    pattern[0] = (rmt_symbol_word_t){{halfPeriod, 1, halfPeriod, 1}}; // Sync bit and start word
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