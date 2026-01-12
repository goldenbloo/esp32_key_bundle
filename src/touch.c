#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "macros.h"
#include "rfid.h"
#include "touch.h"

#define MAX_SUM  1000
#define MAX_SKIP 2000

QueueHandle_t touchInputIsrEvtQueue = NULL;


bool timeAvgCalculated = false, syncBitFound = false, startOk = false;
bool waitForStabilization = true;
uint32_t lastTime = 0;
volatile uint32_t lastIsrTime = 0;
uint32_t sumCnt = 0, timeHigh = 0, timeLow = 0, timeAvg = 0, timeSum = 0, skipCnt = 0;
uint8_t bitCnt = 0, startWordCnt = 0, startWord = 0, parity = 0;
uint32_t data;

void IRAM_ATTR comp_rx_isr_handler(void *arg)
{
    if ((esp_cpu_get_cycle_count() - lastIsrTime ) <  50 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)            
        return;
    
    lastIsrTime = esp_cpu_get_cycle_count();
    touch_input_evt evt = {
        // .currentTime = esp_timer_get_time(),
        .currentTime = esp_cpu_get_cycle_count(),
        .level = gpio_get_level(COMP_RX),
    };
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(touchInputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

void read_metakom_kt2()
{
    uint8_t printEvt = 4;
    xQueueSend(printQueue, &printEvt, 0);
    timeHigh = 0;
    timeLow = 0;
    timeAvg = 0;
    sumCnt = 0;
    timeSum = 0;
    syncBitFound = false;
    timeAvgCalculated = false;
    lastTime = esp_cpu_get_cycle_count();
    skipCnt = 0;
    gpio_set_intr_type(COMP_RX, GPIO_INTR_ANYEDGE);
}

void touch_memory_deferred_task(void* args)
{
    touch_input_evt evt;
    for (;;)
    {
        if (xQueueReceive(touchInputIsrEvtQueue, &evt, portMAX_DELAY))
        {
            if (skipCnt < MAX_SKIP)
            {
                lastTime = evt.currentTime;
                skipCnt++;
                continue;
            }
            
            gpio_set_level(LED_PIN,evt.level);
            uint32_t duration = (evt.currentTime - lastTime) / CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
            lastTime = evt.currentTime;
            
            // printf("%lu\t%d\t%lu\n", duration, evt.level, idx);

            if (sumCnt < MAX_SUM) // Sum time of logic level
            {
                timeSum += duration;
                sumCnt++;                
                continue;
            }

            if (!timeAvgCalculated)
            {
                timeAvg = timeSum / sumCnt; // Calculate half period
                timeAvgCalculated = true;
                // touch_print_t printEvt = {
                //         .evt = 5,
                //         .timeAvg = timeAvg,};
                    // xQueueSend(printQueue, &printEvt, 0);
                // printf("timeAvg = %lu\n", timeAvg);
            }
            else // Average is calculated
            {
                if (evt.level == 1)
                    timeLow = duration;
                else
                    timeHigh = duration;

                if ((timeHigh > ((timeAvg * 1638) >> 10)) && (timeHigh < (timeAvg * 5)) && !syncBitFound) // Sync bit detection (timeHigh > (timeAvg * 1.6))
                {
                    // touch_print_t printEvt = {
                    //     .evt = 0,
                    //     .tick = lastTime,};
                    // xQueueSend(printQueue, &printEvt, 0);
                    syncBitFound = true;
                    bitCnt = 0;
                    startWordCnt = 0;
                    startWord = 0;
                    data = 0;
                    parity = 0;
                    startOk = false;                   
                    continue;
                }

                if (syncBitFound && (evt.level == 1)) // On rising edge determine bit
                {
                    uint8_t bit = timeLow > timeAvg ? 1 : 0;

                    if (!startOk) // Find and check start word
                    {
                        startWord = startWord | (bit << startWordCnt);
                        // touch_print_t printEvt ={
                        //         .evt = 7,
                        //         .tick = lastTime,
                        //         .data = startWord,
                        //         .bitCnt = startWordCnt,};
                        // xQueueSend(printQueue, &printEvt, 0);
                        startWordCnt++;

                        if (startWordCnt == 3) // Start word found
                        {
                            if (startWord == 0b010)
                            {
                                startOk = true;
                                // touch_print_t printEvt ={
                                //         .evt = 1,
                                //         .tick = lastTime,};
                                // xQueueSend(printQueue, &printEvt, 0);
                                // printf("Start word OK\n");
                            }
                            else // Reset on faliure
                            {
                                startWordCnt = 0;
                                startWord = 0;
                                syncBitFound = false;
                                bitCnt = 0;
                                // touch_print_t printEvt ={
                                //         .evt = 2,
                                //         .tick = lastTime,};
                                // xQueueSend(printQueue, &printEvt, 0);
                                // printf("Start word BAD\n");
                            }
                        }
                        
                    }
                    else if (bitCnt < 32) // Start word ok
                    {
                        data = data | (bit << bitCnt);
                        // touch_print_t printEvt ={
                        //         .evt = 6,
                        //         .tick = lastTime,
                        //         .bitCnt = bitCnt,
                        //         .data = data,
                        //         .duration = duration,};
                        //     xQueueSend(printQueue, &printEvt, 0);
                        
                        parity = parity ^ bit; // Calculate parity
                        if ((bitCnt & 7) == 7 && parity != 0)
                        {
                            // touch_print_t printEvt ={
                            //     .evt = 3,
                            //     .tick = lastTime,
                            //     .bitCnt = bitCnt,
                            //     .data = data,};
                            // xQueueSend(printQueue, &printEvt, 0);
                            // printf("Parity BAD\n");
                            bitCnt = 0;
                            syncBitFound = false;
                            parity = 0;
                            startOk = false; 
                        }
                        bitCnt++;
                    }
                    else if (bitCnt == 32)
                    {
                        // touch_print_t printEvt ={
                        //         .evt = 8,
                        //         .tick = lastTime,
                        //         .bitCnt = bitCnt,
                        //         .data = data,};
                        // xQueueSend(printQueue, &printEvt, 0);                        
                        gpio_set_intr_type(COMP_RX, GPIO_INTR_DISABLE);
                    }
                }                
                continue;
            }
        }
    }
}