#include "macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h>
#include "esp_intr_alloc.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_flash.h"
#include "driver/uart.h"
#include "esp_littlefs.h"
#include "rfid.h"
#include "menus.h"
#include <esp_timer.h>
#include "littlefs_records.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"


#define ESP_INTR_FLAG_DEFAULT   0


#define DATA_FILE_PATH "/littlefs/locations.dat"


static const char *TAG = "ISR_offload";
esp_err_t err;

rmt_channel_handle_t tx_chan = NULL;
rmt_encoder_handle_t copy_enc;
rmt_transmit_config_t trans_config  = {
        .loop_count = -1,
        .flags.eot_level = false,
        .flags.queue_nonblocking = true,
    };
rmt_symbol_word_t pulse_pattern[RMT_SIZE];
SemaphoreHandle_t scanSem, scanDoneSem, rfidDoneSem, drawMutex;

u8g2_t u8g2;

uint64_t currentTag;
uint8_t currentTagArray[5];
QueueHandle_t uartQueue, uiEventQueue, modeSwitchQueue;
TaskHandle_t uiHandlerTask = NULL, rfidAutoTxHandler = NULL;
esp_timer_handle_t confirmation_timer_handle, display_delay_timer_handle;
ui_event_e display_delay_cb_arg;

void rfid_deferred_task(void *arg)
{
    rfid_read_event_t evt;
    // uint64_t last_tag = 0;
    char str[70];    
    for (;;)
    {
        if (xQueueReceive(inputIsrEvtQueue, &evt, portMAX_DELAY))
        {    
            {
                ESP_LOGI(TAG, "Level: %d, Time: %lu, idx: %ld, buff: %s", evt.level, evt.ms, evt.idx, int_to_char_bin(str,evt.buf));
                uint64_t long_tag = 0;
                for (uint8_t i = 0; i < 5; i++)
                    long_tag |= ((uint64_t)evt.tag[i] << (8 * i));
                currentTag = long_tag;
                memcpy(currentTagArray, evt.tag, sizeof(currentTagArray));

                int32_t event = EVT_RFID_SCAN_DONE;
                xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
                rfid_disable_rx_tx_tag();
                // xSemaphoreGive()
            }
        }
    }
}

void button_interrupt_handler(void *arg)
{
    static uint32_t lastTick = 0;
    gpio_intr_disable(BUTTON_PIN);
    uint32_t current_tick = esp_cpu_get_cycle_count();
    
    if ((current_tick - lastTick)  < 250 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000 )
    {
        gpio_intr_enable(BUTTON_PIN);
        return;
    }    
    lastTick = current_tick;

    gpio_intr_enable(BUTTON_PIN);
    BaseType_t woken = pdFALSE;
    // xSemaphoreGiveFromISR(modeSwitchSem, &woken);
    xSemaphoreGiveFromISR(scanSem, &woken);
    if (woken == pdTRUE) {
        // If giving the semaphore woke a higher‐priority task, yield now
        portYIELD_FROM_ISR();
    }
    
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        // xSemaphoreGive(scanDoneSem);
        int32_t key = EVT_WIFI_SCAN_DONE;
        xQueueSendToBack(uiEventQueue, &key, pdMS_TO_TICKS(15));

    }
}

void wifi_init()
{
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }
    // 2. Init TCP/IP and default STA
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    // 3. Init Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // 4. Register our scan-done handler
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                        &wifi_event_handler, NULL, NULL);

    // 5. Set Wi-Fi mode to Station
    esp_wifi_set_mode(WIFI_MODE_STA);

    // 6. Start Wi-Fi
    esp_wifi_start();
}



void display_init() 
{
    const char* TAG = "disp_init";
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.i2c.sda = CONFIG_SDA_GPIO; // 21
    u8g2_esp32_hal.bus.i2c.scl = CONFIG_SCL_GPIO; // 22    
    u8g2_esp32_hal_init(u8g2_esp32_hal);    
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C);
    ESP_LOGI(TAG,"Display init");
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFontPosTop(&u8g2);    
    // Draw a frame
    // u8g2_DrawFrame(&u8g2, 0, 0, 128, 64);
    // Set the font
    // u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    // Display some text
    // u8g2_DrawStr(&u8g2, 2, 22, "Hello,");
    // u8g2_DrawStr(&u8g2, 2, 46, "ESP-IDF!");
    // // Draw a line
    // u8g2_DrawLine(&u8g2, 0, 32, 127, 32);
    // Send the buffer to the display
    u8g2_SendBuffer(&u8g2);
}

void gpio_pins_init()
{
    // Signal input GPIO interrupt setup
    gpio_config_t inputSingal_config = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << INPUT_SIGNAL_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&inputSingal_config));
    // Install GPIO ISR service and add the handler for our pin.
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT_SIGNAL_PIN, rfid_read_isr_handler, (void *)INPUT_SIGNAL_PIN));

    // Button GPIO interrupt setup
    gpio_config_t buttonState_config = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&buttonState_config));
    // Install GPIO ISR service and add the handler for our pin.
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, button_interrupt_handler, (void *)BUTTON_PIN));

    // LED pin setup
    gpio_config_t led_io = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_2),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pull-up if needed
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&led_io));
}



void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    char msg[17] = {0};
    // char textBuffer[25] = {0};
    for(;;) 
    {
        // Waiting for UART event.
        if(xQueueReceive(uartQueue, (void* )&event, (TickType_t)portMAX_DELAY)) 
        {            
            switch(event.type) 
            {
                case UART_DATA:
                    if (uart_read_bytes(UART_NUM_0, msg, event.size, portMAX_DELAY) > -1)
                    {
                        int32_t key = -1;
                        
                        // if (key >= 0x30 && key <= 0x39)
                        // {
                        //     key -= 0x30;
                        //     keypad_button_press(key, textBuffer, sizeof(textBuffer), &dev);
                        // }
                        // else if (key == 0x08)                       
                        //     keypad_button_press(10, textBuffer, sizeof(textBuffer), &dev);                        
                        // else if (key == '*')
                        //     keypad_button_press(11, textBuffer, sizeof(textBuffer), &dev);

                        switch (msg[0])
                        {
                        case 'w': // UP
                            key = KEY_UP;
                            break;
                        case 's': // DOWN
                            key = KEY_DOWN;
                            break;
                        case 'a': // LEFT
                            key = KEY_LEFT;
                            break;
                        case 'd': // RIGHT
                            key = KEY_RIGHT;
                            break;
                        case 0xD: // RIGHT
                            key = KEY_ENTER;
                            break;
                        case 0x8: //BACK
                            key = KEY_BACK;
                            break;

                        case 'c': //Delete key
                            key = KEY_CLEAR_CHAR;
                            break;
                        case 'z': //Shift key
                            key = KEY_SHIFT;
                            break;

                        default:
                            if (msg[0] >= 0x30 && msg[0] <= 0x39)                            
                                key = msg[0] - 0x30;                            
                            break;                            
                        }
                        // if (key >= 0)
                        // ESP_LOGI("UART", "Key: %d", key);
                        if (xQueueSendToBack(uiEventQueue, &key, pdMS_TO_TICKS(15))!= pdPASS)
                            {
                                // ESP_LOGI(localTAG,"Can't send key to a queue");
                            }
                    }
                    // Process received data
                    break;
                case UART_FIFO_OVF:
                    // Handle FIFO overflow
                    break;
                case UART_BUFFER_FULL:
                    // Handle buffer full
                    break;
                case UART_PARITY_ERR:
                    // Handle parity error
                    break;
                case UART_FRAME_ERR:
                    // Handle frame error
                    break;
                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}





void app_main(void)
{
    //-----------------------------------------------------
    gpio_set_direction(COIL_VCC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(COIL_VCC_PIN, 0);  
    gpio_set_direction(COIL_OUTPUT_PIN, GPIO_MODE_OUTPUT);
    //-----------------------------------------------------
    wifi_init();
    display_init();
    // ssd1306_display_text(&dev, 0, "hello", 5, false);
    //-----------------------------------------------------

//-----------------------------------------------------------------------------
    // Create the queue. 
    inputIsrEvtQueue = xQueueCreate(20, sizeof(rfid_read_event_t));
    if (inputIsrEvtQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    uiEventQueue = xQueueCreate(10, sizeof(int32_t));
    if (uiEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create key queue");
        return;
    }
    modeSwitchQueue = xQueueCreate(5, sizeof(uint8_t));
    if (modeSwitchQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create key queue");
        return;
    }

//-----------------------------------------------------------------------------
    gpio_pins_init();
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
//-----------------------------------------------------------------------------
    // Create a task to process the deferred events.
    xTaskCreate(rfid_deferred_task, "rfid_deferred_task", 2048, NULL, 4, NULL);    
//-----------------------------------------------------------------------------
    // Create semaphores
    // modeSwitchSem = xSemaphoreCreateBinary();
    scanSem = xSemaphoreCreateBinary();
    scanDoneSem = xSemaphoreCreateBinary();
    rfidDoneSem = xSemaphoreCreateBinary();
    drawMutex = xSemaphoreCreateMutex();    
    // configASSERT(modeSwitchSem != NULL);
    configASSERT(scanSem != NULL);
    configASSERT(scanDoneSem != NULL);
    configASSERT(rfidDoneSem != NULL);
    configASSERT(drawMutex != NULL);


    // Create the worker task
    // xTaskCreate(mode_switch_task, "mode_switch", 2048, 0, 0, NULL);
    // xTaskCreate(scan_wifi_and_tag, "scan_wifi_&_tag", 4096, 0, 1, NULL);
    // ESP_ERROR_CHECK(rmt_disable(tx_chan));
    // xSemaphoreGive(modeSwitchSem);
//-----------------------------------------------------------------------------
    //LittleFS
    littlefs_init();
//-----------------------------------------------------------------------------
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(UART_NUM_0, &uart_config);
    const int uart_buffer_size = (1024 * 2);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, uart_buffer_size, 0, 10, &uartQueue, 0);
    
    xTaskCreate(uart_event_task, "uart_receive_task", 1024, NULL, 2, NULL);
    // xTaskCreate(textbox_dispay, "tbt", 2048, NULL, 1, NULL);


    const esp_timer_create_args_t confirmation_timer_args = {
        .callback = &confirmation_timer_callback,
        .name = "keypad_timer",                    
        };
    err = esp_timer_create(&confirmation_timer_args, &confirmation_timer_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create confirmation timer: %s", esp_err_to_name(err));
        return;
    }

    const esp_timer_create_args_t display_delay_timer_args = {
        .callback = &display_delay_timer_callback,
        .name = "display_delay",
        .arg = &display_delay_cb_arg,
        };
    err = esp_timer_create(&display_delay_timer_args, &display_delay_timer_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create confirmation timer: %s", esp_err_to_name(err));
        return;
    }
    xTaskCreate(ui_handler_task, "ui_handler_task", 4096, NULL, 3, &uiHandlerTask);

    xTaskCreate(tag_tx_cycle_callback, "tag_tx_cycle_callback", 2048, NULL, 0, &rfidAutoTxHandler);
    vTaskSuspend(rfidAutoTxHandler);    
    
}
