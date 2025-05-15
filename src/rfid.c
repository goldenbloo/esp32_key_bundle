#include "rfid.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "macros.h"

QueueHandle_t inputIsrEvtQueue = NULL;

void inline syncErrorFunc(manchester_t *m)
{
    m->checkNextEdge = false;
    m->bitIsReady = false;
    m->checkNextEdge = false;
    m->tagInputBuff = 0;
}

// const uint8_t nibble_reverse_table[16] = {
//     0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
//     0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF
// };
const uint8_t parity_row_table[16] = {
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0};

void IRAM_ATTR rfid_read_isr_handler(void *arg)
{
    static uint32_t lastTick = 0;
    static bool lastLevel = 0;
    static manchester_t m;
    // static uint32_t idx = 0;
    gpio_intr_disable(INPUT_SIGNAL_PIN);
    uint32_t currTick = esp_cpu_get_cycle_count();
    rfid_read_event_t evt;
    //  if (gptimer_get_raw_count(signalTimer, &evt.ms) != ESP_OK)
    //     evt.ms = 0; // Default to 0 if retrieval fails
    // gptimer_set_raw_count(signalTimer, 0);
    evt.level = gpio_get_level(INPUT_SIGNAL_PIN); // Capture the current level

    uint32_t tickDiff = (currTick - lastTick);
    evt.ms = tickDiff / CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

    // if (tickDiff > PERIOD_HIGH * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ || tickDiff < PERIOD_HALF_LOW *CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
    // if (evt.ms > PERIOD_HIGH || evt.ms < PERIOD_HALF_LOW )
    if (evt.ms < PERIOD_HALF_LOW || (lastLevel == evt.level))
    {
        gpio_intr_enable(INPUT_SIGNAL_PIN);
        return;
    }
    // // evt.ms = esp_log_timestamp();
    lastTick = currTick;
    lastLevel = evt.level;
    // evt.idx = idx++;

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
            // gpio_set_level(LED_PIN, m.currentBit);
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
                // gpio_set_level(LED_PIN, m.currentBit);
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
            for (uint8_t i = 5; i < 55; i += 5)
            {
                uint8_t parity = 0;
                for (uint8_t j = 0; j < 5; j++)
                    parity ^= (m.tagInputBuff >> (i + j)) & 1;
                if (parity != 0)
                {
                    isParityOk = false;
                    break;
                }
            }
            // Parity by columns
            for (uint8_t i = 1; i < 5; i++)
            {
                uint8_t parity = 0;
                for (uint8_t j = 0; j < 55; j += 5)
                    parity ^= (m.tagInputBuff >> (i + j)) & 1;
                if (parity != 0)
                {
                    isParityOk = false;
                    break;
                }
            }
            if (isParityOk)
            {
                uint8_t nibbles[10];
                for (uint8_t i = 0; i < 10; i++)
                {
                    uint8_t shift = (i + 1) * 5 + 1;
                    nibbles[i] = (m.tagInputBuff >> shift) & 0x0F;
                }
                for (uint8_t byte = 0; byte < 5; byte++)                
                    evt.tag[byte] = (nibbles[2 * byte + 1] << 4) | (nibbles[2 * byte]);                
                // evt.tag[i / 2] |= ((m.tagInputBuff >> ((i + 1) * 5 + 1) ) & 0x0F) << (i % 2);
                // ESP_LOGI(TAG, "tag:\t %#lx", tagId);
            }
        }
    }
    // Enqueue the event
end:
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(inputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        gpio_intr_enable(INPUT_SIGNAL_PIN);
        portYIELD_FROM_ISR();
    }
    gpio_intr_enable(INPUT_SIGNAL_PIN);
}

char *int_to_char_bin(char *str, uint64_t num)
{
    for (uint8_t i = 0; i < 64; i++)
    {
        str[63 - i] = ((num >> i) & 1) + 0x30;
    }
    str[64] = 0;
    return str;
}

uint64_t tagId_to_raw_tag(uint8_t *tagArr)
{
    uint64_t rawTag = 0x1FF;
    uint8_t nibbles[11]; // 10 nibbles 4 bits - data, 1 bit - parity;  last nibble 4 bits - column parity and 1 bit - stop
    for (uint8_t i = 0; i < 10; i++)
    {
        uint8_t nibble = (tagArr[i / 2] >> 4 * (i % 2)) & 0xF;
        nibbles[9-i] = (nibble << 1) | parity_row_table[nibble];
        nibbles[10] ^= nibble;
    }
    nibbles[10] &= 0xFE; // Clear bit 0 for stop bit

    for (uint8_t i = 0; i <11; i++)
        rawTag = (rawTag << 5) | (nibbles[i]);      
    
    return rawTag;
}

