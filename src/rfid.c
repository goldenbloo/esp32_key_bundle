#include "rfid.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "esp_log.h"
#include "string.h"
#include "macros.h"
#include "menus.h"
#include "littlefs_records.h"

QueueHandle_t rfidInputIsrEvtQueue = NULL;
volatile uint32_t lastIsrTime = 0;
static manchester_t m;


void rfid_deferred_task(void *arg)
{
    rfid_input_event_t evt;
 
    for (;;)
    {
        if (xQueueReceive(rfidInputIsrEvtQueue, &evt, portMAX_DELAY))
        {    
           manchester_read(evt.level, evt.duration);
        }
    }
}

void inline syncErrorFunc(manchester_t *m)
{
    memset(m, 0, sizeof(manchester_t));
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
    uint32_t duration = (esp_cpu_get_cycle_count() - lastIsrTime) / CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ; // convert to microseconds
    if ((duration) < 50)            
        return;
    
    lastIsrTime = esp_cpu_get_cycle_count();
    rfid_input_event_t evt = {        
        .duration = duration,
        .level = !((GPIO.in >> (RFID_RX )) & 1), // .in1 - GPIO 32-39
    };

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(rfidInputIsrEvtQueue, &evt, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}


void manchester_read(uint8_t level, uint32_t duration)
{  
    const uint32_t T = 256; // 1/2 of data rate 
    //---------------------------------------------------------------
    if ((duration > MUL_BY_1_25(2*T)) || (duration < MUL_BY_0_75(T)))
    {
        syncErrorFunc(&m);    
        return;
    }
    // If Manchester sync is not established and signal state time between PREIOD_LOW and PERIOD_HIGH
    // Establish sync and set current bit
    if (!m.isSynced)
    {
        if ((duration > MUL_BY_0_75(2*T) ) && (duration < MUL_BY_1_25(2*T)))
        {
            m.isSynced = true;
            m.currentBit = !level;
        }
    }
    // If m sync is established and signal state time is more than PERIOD_LOW
    else
    {
        m.lastBit = m.currentBit;
        // If signal state time is more PERIOD_LOW
        // Set currentBit = !lastBit
        if (duration > MUL_BY_0_75(2*T))
        {
            if (m.checkNextEdge) // shouldn’t happen: we expected a half-edge but got a full one
            {
                syncErrorFunc(&m);                
                return;
            }
            m.currentBit = !m.lastBit;
            m.bitIsReady = true;
            // gpio_set_level(LED_PIN, m.currentBit);
        }
        // If m sync is established and counter is more than PERIOD_HALF_LOW
        else if (duration > MUL_BY_0_75(T))
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
            return;
        }                    
    }
    // We are reading full tag with start and stop bits first into a buffer and then check if all is good
    if (m.bitIsReady)
    {
        m.bitIsReady = false;
        m.tagInputBuff = (m.tagInputBuff << 1) | m.currentBit; // Shift to left and add current bit if bit is ready        

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
                memset(&currentKeyData, 0, sizeof(currentKeyData));
                for (uint8_t byte = 0; byte < 5; byte++)                
                    currentKeyData.rfid.id[byte] = (nibbles[2 * byte + 1] << 4) | (nibbles[2 * byte]);
                currentKeyType = KEY_TYPE_EM4100_MANCHESTER_64; 
                char str[70];
                ESP_LOGI("manchester", "tag:0x%010llX, buff: %s", currentKeyData.value, int64_to_char_bin(str,currentKeyData.value));
                ui_event_e event = EVT_KEY_SCAN_DONE;
                xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
                rfid_disable_rx_tx();
                xQueueReset(rfidInputIsrEvtQueue);
            }            
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

uint64_t rfid_arr_tag_to_raw_bitstream(uint8_t *tagArr)
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

void rfid_array_to_rmt(rmt_symbol_word_t *rmtArr, uint64_t rawTag)
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

void rfid_int_to_array(uint64_t tag, uint8_t tagArr[])
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
    ESP_ERROR_CHECK(gpio_intr_enable(RFID_RX));
    gpio_set_level(LED_PIN, 1);
    // Reset manchester decoder
    syncErrorFunc(&m);
}

void rfid_enable_tx_raw_tag(uint64_t rawTag)
{
    const char* TAG = "enable tx tag";
    // Disable GPIO input signal interrupt
    ESP_ERROR_CHECK(gpio_intr_disable(RFID_RX));
    // Disable coil VCC
    gpio_set_level(COIL_VCC_PIN, 0);
    // ESP_ERROR_CHECK(rmt_tx_wait_all_done(rfid_tx_ch, portMAX_DELAY));
    // Trying to disable RMT
    esp_err_t err = rmt_disable(rfid_tx_ch);
    // Set raw tag into rmt symbol array
    rfid_array_to_rmt(pulse_pattern, rawTag);
    // Start RMT TX
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    ESP_ERROR_CHECK(rmt_enable(rfid_tx_ch));
    ESP_ERROR_CHECK(rmt_transmit(rfid_tx_ch, copy_enc, pulse_pattern, sizeof(pulse_pattern), &rfid_tx_config));
    // ESP_LOGI(TAG, "rmt tx tag");
    gpio_set_level(LED_PIN, 1);
}

void rfid_disable_rx_tx()
{
    // const char* TAG = "diasable tx tag";
    ESP_ERROR_CHECK(gpio_intr_disable(RFID_RX));
    gpio_set_level(COIL_VCC_PIN, 0);
    rmt_disable(rfid_tx_ch);
    gpio_set_level(LED_PIN, 0);
}

