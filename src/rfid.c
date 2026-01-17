#include "rfid.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "string.h"
#include "macros.h"
#include "menus.h"

QueueHandle_t rfidInputIsrEvtQueue = NULL;

uint64_t currentTag;
uint8_t currentTagArray[5];
void rfid_deferred_task(void *arg)
{
    rfid_read_event_t evt;
    // uint64_t last_tag = 0;
    char str[70];    
    for (;;)
    {
        if (xQueueReceive(rfidInputIsrEvtQueue, &evt, portMAX_DELAY))
        {    
            {
                ESP_LOGI("ISR_offload", "Level: %d, Time: %lu, idx: %ld, buff: %s", evt.level, evt.ms, evt.idx, int64_to_char_bin(str,evt.buf));
                uint64_t long_tag = 0;
                for (uint8_t i = 0; i < 5; i++)
                    long_tag |= ((uint64_t)evt.tag[i] << (8 * i));
                currentTag = long_tag;
                memcpy(currentTagArray, evt.tag, sizeof(currentTagArray));

                int32_t event = EVT_KEY_SCAN_DONE;
                xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
                rfid_disable_rx_tx_tag();
                // xSemaphoreGive()
            }
        }
    }
}

void inline syncErrorFunc(manchester_t *m)
{
    m->checkNextEdge = false;
    m->bitIsReady = false;
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
    static uint32_t idx = 0;
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
    evt.idx = idx++;

    //---------------------------------------------------------------
    if ((evt.ms > PERIOD_HIGH) || (evt.ms < PERIOD_HALF_LOW))
    {
        syncErrorFunc(&m);
        // goto end;
        gpio_intr_enable(INPUT_SIGNAL_PIN);
        return;
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
            if (m.checkNextEdge) // shouldn’t happen: we expected a half-edge but got a full one
            {
                syncErrorFunc(&m);
                gpio_intr_enable(INPUT_SIGNAL_PIN);
                return;
            }
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
        {
            syncErrorFunc(&m);
            gpio_intr_enable(INPUT_SIGNAL_PIN);
            return;
        }                    
    }
    // We are reading full tag with start and stop bits first into a buffer and then check if all is good
    if (m.bitIsReady)
    {
        m.bitIsReady = false;
        m.tagInputBuff = (m.tagInputBuff << 1) | m.currentBit; // Shift to left and add current bit if bit is ready
        evt.buf = m.tagInputBuff;

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
                m.tagInputBuff = 0;    
                gpio_intr_enable(INPUT_SIGNAL_PIN);
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                xQueueSendToBackFromISR(rfidInputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
                if (xHigherPriorityTaskWoken)                
                    portYIELD_FROM_ISR();                
            }
            else 
            {
                gpio_intr_enable(INPUT_SIGNAL_PIN);
                return;
            }
        }
        else
        {
            gpio_intr_enable(INPUT_SIGNAL_PIN);
            return;
        }
    }
}


void manchester_read (void *arg)
{
    static uint32_t lastTick = 0;
    static bool lastLevel = 0;
    static manchester_t m;
    static uint32_t idx = 0;
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
    evt.idx = idx++;

    //---------------------------------------------------------------
    if ((evt.ms > PERIOD_HIGH) || (evt.ms < PERIOD_HALF_LOW))
    {
        syncErrorFunc(&m);
        // goto end;
        gpio_intr_enable(INPUT_SIGNAL_PIN);
        return;
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
            if (m.checkNextEdge) // shouldn’t happen: we expected a half-edge but got a full one
            {
                syncErrorFunc(&m);
                gpio_intr_enable(INPUT_SIGNAL_PIN);
                return;
            }
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
        {
            syncErrorFunc(&m);
            gpio_intr_enable(INPUT_SIGNAL_PIN);
            return;
        }                    
    }
    // We are reading full tag with start and stop bits first into a buffer and then check if all is good
    if (m.bitIsReady)
    {
        m.bitIsReady = false;
        m.tagInputBuff = (m.tagInputBuff << 1) | m.currentBit; // Shift to left and add current bit if bit is ready
        evt.buf = m.tagInputBuff;

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
                m.tagInputBuff = 0;    
                gpio_intr_enable(INPUT_SIGNAL_PIN);
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                xQueueSendToBackFromISR(rfidInputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
                if (xHigherPriorityTaskWoken)                
                    portYIELD_FROM_ISR();                
            }
            else 
            {
                gpio_intr_enable(INPUT_SIGNAL_PIN);
                return;
            }
        }
        else
        {
            gpio_intr_enable(INPUT_SIGNAL_PIN);
            return;
        }
    }
}


char *int64_to_char_bin(char *str, uint64_t num) // Minumal char buffer size is 65
{
    for (uint8_t i = 0; i < 64; i++)
    {
        str[63 - i] = ((num >> i) & 1) + 0x30;
    }
    str[64] = 0;
    return str;
}

char *int32_to_char_bin(char *str, uint32_t num) // Minumal char buffer size is 33
{
    for (uint8_t i = 0; i < 32; i++)
    {
        str[31 - i] = ((num >> i) & 1) + 0x30;
    }
    str[32] = 0;
    return str;
}

uint64_t rfid_arr_tag_to_raw_tag(uint8_t *tagArr)
{
    uint64_t rawTag = 0x1FF;
    uint8_t nibbles[11]; // 10 nibbles 4 bits - data, 1 bit - parity;  last nibble 4 bits - column parity and 1 bit - stop
    uint8_t column_parity = 0;
    for (uint8_t i = 0; i < 10; i++)
    {
        uint8_t nibble = (tagArr[i / 2] >> (4 * 1 - (i % 2))) & 0xF;
        nibbles[9-i] = (nibble << 1) | parity_row_table[nibble];
        column_parity ^= nibble;        
    }
    nibbles[10] &= column_parity << 1; // Clear bit 0 for stop bit

    for (uint8_t i = 0; i <11; i++)
        rawTag = (rawTag << 5) | (nibbles[i] & 0x1F);      
    
    return rawTag;
}

void rfid_raw_tag_to_rmt(rmt_symbol_word_t *rmtArr, uint64_t rawTag)
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

void rfid_tag_to_array(uint64_t tag, uint8_t tagArr[])
{
    for (uint8_t i = 0; i < 5; ++i) {
    tagArr[i] = (tag >> (i * 8)) & 0xFF;
}
}

uint64_t rfid_array_to_tag(uint8_t tagArr[])
{
    uint64_t long_tag = 0;
    for (uint8_t i = 0; i < 5; i++)
        long_tag |= ((uint64_t)tagArr[i] << (8 * i));
    return long_tag;
}

void rfid_enable_rx_tag()
{   
    const char* TAG = "enable read tag";
    esp_err_t err = rmt_disable(rfid_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    // Set rmt symbol array to transmit 125 khz signal
    ESP_ERROR_CHECK(rmt_enable(rfid_tx_ch));
    for (uint8_t i = 0; i < 64; i++)
    {
        pulse_pattern[i].duration0 = 4;
        pulse_pattern[i].duration1 = 4;
        pulse_pattern[i].level0 = 1;
        pulse_pattern[i].level1 = 0;
    }
    // Start RMT TX with 125 khz signal
    ESP_ERROR_CHECK(rmt_transmit(rfid_tx_ch, copy_enc, pulse_pattern, sizeof(pulse_pattern), &rfid_tx_config));
    // ESP_LOGI(TAG, "rmt tx carrier");
    // Enable coil VCC
    gpio_set_level(COIL_VCC_PIN, 1);
    // Enable GPIO input signal interrupt
    ESP_ERROR_CHECK(gpio_intr_enable(INPUT_SIGNAL_PIN));
    gpio_set_level(LED_PIN, 1);
}

void rfid_enable_tx_raw_tag(uint64_t rawTag)
{
    const char* TAG = "enable tx tag";
    // Disable GPIO input signal interrupt
    ESP_ERROR_CHECK(gpio_intr_disable(INPUT_SIGNAL_PIN));
    // Disable coil VCC
    gpio_set_level(COIL_VCC_PIN, 0);
    // ESP_ERROR_CHECK(rmt_tx_wait_all_done(rfid_tx_ch, portMAX_DELAY));
    // Trying to disable RMT
    esp_err_t err = rmt_disable(rfid_tx_ch);
    // Set raw tag into rmt symbol array
    rfid_raw_tag_to_rmt(pulse_pattern, rawTag);
    // Start RMT TX
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    ESP_ERROR_CHECK(rmt_enable(rfid_tx_ch));
    ESP_ERROR_CHECK(rmt_transmit(rfid_tx_ch, copy_enc, pulse_pattern, sizeof(pulse_pattern), &rfid_tx_config));
    // ESP_LOGI(TAG, "rmt tx tag");
    gpio_set_level(LED_PIN, 1);
}

void rfid_disable_rx_tx_tag()
{
    // const char* TAG = "diasable tx tag";
    ESP_ERROR_CHECK(gpio_intr_disable(INPUT_SIGNAL_PIN));
    gpio_set_level(COIL_VCC_PIN, 0);
    rmt_disable(rfid_tx_ch);
    gpio_set_level(LED_PIN, 0);
}