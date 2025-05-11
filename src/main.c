#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h> // For memset
#include "esp_intr_alloc.h"
#include <manchester.h>

#define INPUT_SIGNAL_PIN  23
#define LED_PIN           2
#define COIL_SIGNAL_PIN   19

#define PERIOD            512
#define PERIOD_LOW        387
#define PERIOD_HIGH       640
#define PERIOD_HALF       256
#define PERIOD_HALF_LOW   140
#define PERIOD_HALF_HIGH  386


#define ESP_INTR_FLAG_DEFAULT 0

static const uint8_t nibble_reverse_table[16] = {
    0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
    0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF
};

static const char *TAG = "ISR_offload";
static QueueHandle_t inputIsrEvtQueue = NULL;
gptimer_handle_t signalTimer = NULL;
static manchester_t m = {0,0,0,0,0,0};

uint32_t idx = 0;
uint32_t lastTick = 0;
bool lastLevel = 0;

typedef struct {
    bool level;
    uint32_t ms;  // Optional: store a ms if needed
    uint32_t idx;
    uint64_t buf;
    uint64_t tag;
} rfid_read_event_t;



static void IRAM_ATTR rfid_read_isr_handler(void *arg)
{    
    gpio_intr_disable(INPUT_SIGNAL_PIN);
    uint32_t currTick = esp_cpu_get_cycle_count(); 
    rfid_read_event_t evt;
    //  if (gptimer_get_raw_count(signalTimer, &evt.ms) != ESP_OK)    
    //     evt.ms = 0; // Default to 0 if retrieval fails  
    // gptimer_set_raw_count(signalTimer, 0); 
    evt.level = gpio_get_level(INPUT_SIGNAL_PIN); // Capture the current level
   
    uint32_t tickDiff = (currTick - lastTick);
    evt.ms = tickDiff / CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ ;
   
    // if (tickDiff > PERIOD_HIGH * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ || tickDiff < PERIOD_HALF_LOW *CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)     
    // if (evt.ms > PERIOD_HIGH || evt.ms < PERIOD_HALF_LOW ) 
    if (evt.ms < PERIOD_HALF_LOW || (lastLevel == evt.level) ) 
    {
        gpio_intr_enable(INPUT_SIGNAL_PIN);        
        return;
    }
    // // evt.ms = esp_log_timestamp();
    lastTick = currTick;    
    lastLevel = evt.level;
    evt.idx = idx++;    
    gpio_set_level(LED_PIN, evt.level);  
  //---------------------------------------------------------------
    if ((evt.ms > PERIOD_HIGH) || (evt.ms < PERIOD_HALF_LOW))
    {
         syncErrorFunc(&m);
         goto end;
    }

    // If Manchester sync is not established and signal state time between PREIOD_LOW and PERIOD_HIGH
    // Establish sync and set current bit
    if (!m.isSynced)
    {
        if ((evt.ms > PERIOD_LOW) && (evt.ms < PERIOD_HIGH))
        {
            m.isSynced = true;
            m.currentBit = evt.level;
        }
    }
    // If m sync is established and signal state time is more than PERIOD_LOW
    else
    {
        m.lastBit = m.currentBit;
        // If signal state time is more PERIOD_LOW
        // Set currentBit = !lastBit
        if (evt.ms > PERIOD_LOW)
        {
            if (m.checkNextEdge)
                syncErrorFunc(&m);
            m.currentBit = !m.lastBit;
            m.bitIsReady = true;
            gpio_set_level(LED_PIN, m.currentBit);
        }
        // If m sync is established and counter is more than PERIOD_HALF_LOW
        else if (evt.ms > PERIOD_HALF_LOW)
        {
            if (!m.checkNextEdge)
                m.checkNextEdge = true;
            else
            {
                m.currentBit = m.lastBit;
                m.bitIsReady = true;
                m.checkNextEdge = false;
                gpio_set_level(LED_PIN, m.currentBit);

            }
        }
        else
            syncErrorFunc(&m);
            
    }
    // We are reading full tag with start and stop bits first into a buffer and then check if all is good
    if (m.bitIsReady)
    {
        m.bitIsReady = false;
        m.tagInputBuff = (m.tagInputBuff << 1) | m.currentBit; // Shift to left and add current bit if bit is ready
        evt.buf = m.tagInputBuff;
        // readCount++;
        // if (readCount > 64)
        // {
        //     // uint32_t tagId;
        //     readCount = 0;
        //     // for (uint8_t i = 6; i < 55; i += 5)
        //     //     tagId = (tagId << 4) | ((m.tagInputBuff >> i) & 0x0F);
        //     ESP_LOGI(TAG, "buff__: %#llx", m.tagInputBuff);
        // }

        if ((m.tagInputBuff & 0xFF80000000000000) == 0xFF80000000000000) // Check if fist 9 bits are all 1
        {
            // ESP_LOGI(TAG, "buff:\t %#llx", m.tagInputBuff);
            bool isParityOk = true;
            // Parity by rows
            for (uint8_t i = 5; i <55; i += 5)
            {
                uint8_t parity = 0;
                for (uint8_t j = 0; j < 5; j++)
                    parity ^= (m.tagInputBuff >> (i+j)) & 1;                        
                if (parity != 0)
                {
                    isParityOk = false;
                    break;
                }
            }
            // Parity by columns
            for(uint8_t i = 1; i < 5; i++)
            {
                uint8_t parity = 0;
                for (uint8_t j = 0; j < 55; j+= 5)
                parity ^= (m.tagInputBuff >> (i+j)) & 1;
                if (parity != 0)
                {
                    isParityOk = false;
                    break;
                }                        
            }
            if (isParityOk)
            {
                uint64_t tagId = 0;
                for (uint8_t i = 6; i < 55; i += 5)
                    tagId = (tagId << 4) | nibble_reverse_table[((m.tagInputBuff >> i) & 0x0F)];
                evt.tag = tagId;
                // ESP_LOGI(TAG, "tag:\t %#lx", tagId);
            }
        }

    }
    // Enqueue the event
end:
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(inputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
    if(xHigherPriorityTaskWoken)
    { 
        gpio_intr_enable(INPUT_SIGNAL_PIN);
        portYIELD_FROM_ISR();
    }
    gpio_intr_enable(INPUT_SIGNAL_PIN);
    
}

static void rfid_deferred_task(void *arg)
{
    rfid_read_event_t evt;
    static uint8_t readCount = 0;    
    uint32_t tick1 = 0;
    // char str[65];
    
    for (;;)
    {
        if (xQueueReceive(inputIsrEvtQueue, &evt, portMAX_DELAY))
        tick1 = esp_cpu_get_cycle_count();
        // if (xQueueReceive(inputIsrEvtQueue, &evt, 100000))
        {
            // ESP_LOGI(TAG, "Level: %d, Tick: %llu", evt.level, evt.ms);
            // gpio_set_level(LED_PIN, evt.level);     
            // readCount++;
             if (evt.tag > 0)       
            // {
                // ESP_LOGI(TAG, "Level: %d, Time: %lu, idx: %ld, buff: %s", evt.level, evt.ms, evt.idx, int_to_char_bin(str,evt.buf));
                 ESP_LOGI(TAG, "tag: %#llx", evt.tag);
            // }

            // if ((evt.ms > PERIOD_HIGH) || (evt.ms < PERIOD_HALF_LOW))
            // {
            //      syncErrorFunc(&m);
            //      continue;
            // }
               
            // // If Manchester sync is not established and signal state time between PREIOD_LOW and PERIOD_HIGH
            // // Establish sync and set current bit
            // if (!m.isSynced)
            // {
            //     if ((evt.ms > PERIOD_LOW) && (evt.ms < PERIOD_HIGH))
            //     {
            //         m.isSynced = true;
            //         m.currentBit = evt.level;
            //     }
            // }
            // // If m sync is established and signal state time is more than PERIOD_LOW
            // else
            // {
            //     m.lastBit = m.currentBit;
            //     // If signal state time is more PERIOD_LOW
            //     // Set currentBit = !lastBit
            //     if (evt.ms > PERIOD_LOW)
            //     {
            //         if (m.checkNextEdge)
            //             syncErrorFunc(&m);
            //         m.currentBit = !m.lastBit;
            //         m.bitIsReady = true;
            //         gpio_set_level(LED_PIN, m.currentBit);
            //     }
            //     // If m sync is established and counter is more than PERIOD_HALF_LOW
            //     else if (evt.ms > PERIOD_HALF_LOW)
            //     {
            //         if (!m.checkNextEdge)
            //             m.checkNextEdge = true;
            //         else
            //         {
            //             m.currentBit = m.lastBit;
            //             m.bitIsReady = true;
            //             m.checkNextEdge = false;
            //             gpio_set_level(LED_PIN, m.currentBit);

            //         }
            //     }
            //     else
            //         syncErrorFunc(&m);
                    
            // }
            // // We are reading full tag with start and stop bits first into a buffer and then check if all is good
            // if (m.bitIsReady)
            // {
            //     m.bitIsReady = false;
            //     m.tagInputBuff = (m.tagInputBuff << 1) | m.currentBit; // Shift to left and add current bit if bit is ready
                
            //     // readCount++;
            //     // if (readCount > 64)
            //     // {
            //     //     // uint32_t tagId;
            //     //     readCount = 0;
            //     //     // for (uint8_t i = 6; i < 55; i += 5)
            //     //     //     tagId = (tagId << 4) | ((m.tagInputBuff >> i) & 0x0F);
            //     //     ESP_LOGI(TAG, "buff__: %#llx", m.tagInputBuff);
            //     // }

            //     if ((m.tagInputBuff & 0xFF80000000000000) == 0xFF80000000000000) // Check if fist 9 bits are all 1
            //     {
            //         // ESP_LOGI(TAG, "buff:\t %#llx", m.tagInputBuff);
            //         bool isParityOk = true;
            //         // Parity by rows
            //         for (uint8_t i = 5; i <56; i += 5)
            //         {
            //             uint8_t parity = 0;
            //             for (uint8_t j = 0; j < 5; j++)
            //                 parity ^= (m.tagInputBuff >> (i+j)) & 1;                        
            //             if (parity != 0)
            //             {
            //                 isParityOk = false;
            //                 break;
            //             }
            //         }
            //         // Parity by columns
            //         for(uint8_t i = 1; i < 5; i++)
            //         {
            //             uint8_t parity = 0;
            //             for (uint8_t j = 0; j < 55; j+= 5)
            //             parity ^= (m.tagInputBuff >> (i+j)) & 1;
            //             if (parity != 0)
            //             {
            //                 isParityOk = false;
            //                 break;
            //             }                        
            //         }
            //         if (isParityOk)
            //         {
            //             uint32_t tagId = 0;
            //             for (uint8_t i = 6; i < 55; i += 5)
            //                 tagId = (tagId << 4) | ((m.tagInputBuff >> i) & 0x0F);
            //             ESP_LOGI(TAG, "tag:\t %#lx", tagId);
            //         }
            //     }
                
                
            // }
            // ESP_LOGI(TAG, "tag:\t %lu", esp_cpu_get_cycle_count() - tick1); 
            // vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void app_main(void)
{
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
        .gpio_num       = COIL_SIGNAL_PIN,       // Replace with your desired GPIO
        .duty           = 2,                 // 50% duty cycle for 1-bit resolution
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 512));
//-----------------------------------------------------------------------------
// General Purpouse Timer Setup
// Timer for impulse time counting
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &signalTimer));
    ESP_ERROR_CHECK(gptimer_enable(signalTimer));
    ESP_ERROR_CHECK(gptimer_start(signalTimer));
    // Create the queue. Here we allow up to 25 events to be queued.
    inputIsrEvtQueue = xQueueCreate(1000, sizeof(rfid_read_event_t));
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
    .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable pull-up if needed
    .pull_down_en = GPIO_PULLDOWN_DISABLE 
};
ESP_ERROR_CHECK(gpio_config(&led_io));

//-----------------------------------------------------------------------------
// Create a task to process the deferred events.
    
    xTaskCreate(rfid_deferred_task, "rfid_deferred_task", 2048, NULL, 1, NULL);


}