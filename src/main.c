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
#include "touch.h"


#define ESP_INTR_FLAG_DEFAULT   0


#define DATA_FILE_PATH "/littlefs/locations.dat"


static const char *TAG = "ISR_offload";
esp_err_t err;

rmt_channel_handle_t rfid_tx_ch = NULL, touch_tx_ch = NULL;
rmt_encoder_handle_t copy_enc;
rmt_transmit_config_t rfid_tx_config  = {
        .loop_count = -1,
        .flags.eot_level = 0,
        .flags.queue_nonblocking = true,
    };    
rmt_transmit_config_t touch_tx_config = {
    .loop_count = -1,
    .flags.queue_nonblocking = true,
    .flags.eot_level = 1, // Inverted 
};
rmt_symbol_word_t pulse_pattern[RMT_SIZE];
SemaphoreHandle_t scanSem, scanDoneSem, rfidDoneSem, scrollDeleteSem, drawMutex;
u8g2_t u8g2;

QueueHandle_t uartQueue, uiEventQueue, modeSwitchQueue, printQueue;
TaskHandle_t uiHandlerTask = NULL, rfidAutoTxHandler = NULL;
esp_timer_handle_t confirmation_timer_handle, display_delay_timer_handle, keypad_poll_handler;
ui_event_e display_delay_cb_arg;



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

    // // Keypad pin setup
    // gpio_config_t keypad_out = {
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pin_bit_mask = (1ULL << CD4017_CLK) | (1ULL << CD4017_RST),
    //     .pull_up_en = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .intr_type = GPIO_INTR_DISABLE,};
    // ESP_ERROR_CHECK(gpio_config(&keypad_out));

    // gpio_config_t keypad_in = {
    //     .mode = GPIO_MODE_INPUT,
    //     .pin_bit_mask = (1ULL << KEYPAD_C0) | (1ULL << KEYPAD_C1),
    //     .pull_up_en = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .intr_type = GPIO_INTR_DISABLE,};
    // ESP_ERROR_CHECK(gpio_config(&keypad_in));

    // Touch memory pins
    // Pullup pin
    gpio_set_level(PULLUP_PIN, 0); // NPN pullup enabled
    gpio_config_t touch_pullup_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PULLUP_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,      
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,};
    ESP_ERROR_CHECK(gpio_config(&touch_pullup_config));

    // Metakom transmit
    gpio_set_level(METAKOM_TX, 0);
    gpio_config_t matakom_tx_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << METAKOM_TX),
        .pull_up_en = GPIO_PULLUP_ENABLE,      
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,};
    ESP_ERROR_CHECK(gpio_config(&matakom_tx_config));
    
    // One Wire transmit
    gpio_set_level(OWI_TX, 0);
    gpio_config_t owi_tx_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << OWI_TX),
        .pull_up_en = GPIO_PULLUP_ENABLE,      
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,};
    ESP_ERROR_CHECK(gpio_config(&owi_tx_config));
    
    // Comparator output pin
    gpio_config_t comp_rx_config = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << COMP_RX),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,};
    ESP_ERROR_CHECK(gpio_config(&comp_rx_config));
    ESP_ERROR_CHECK(gpio_isr_handler_add(COMP_RX, comp_rx_isr_handler, (void *)COMP_RX));
    gpio_set_intr_type(COMP_RX, GPIO_INTR_DISABLE);

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

const ui_event_e keyLookup[10][2] = {
    {KEY_9, KEY_SHIFT},      // ROW 0
    {KEY_0, KEY_CLEAR_CHAR}, // ROW 1
    {KEY_8, KEY_7},          // ROW 2
    {KEY_3, KEY_2},          // ROW 3
    {KEY_LEFT, KEY_ENTER},   // ROW 4
    {0, 0},                  // ROW 5
    {KEY_5, KEY_4},          // ROW 6
    {KEY_6, KEY_1},          // ROW 7
    {KEY_UP, KEY_BACK},      // ROW 8
    {KEY_DOWN, KEY_RIGHT},   // ROW 9
};

uint8_t keypadRow = 0;
uint8_t debounceCnt[10][2] = {0};
bool pressedKey[10][2] = {0};          

void keypad_poll_callback()
{
    const uint8_t treshold = 4;
    uint8_t cols[2];
    cols[0] = gpio_get_level(KEYPAD_C0);
    cols[1] = gpio_get_level(KEYPAD_C1);

    for (uint8_t i = 0; i < 2; i++)
    {
        if (cols[i] == 1)
        {   
            if (debounceCnt[keypadRow][i] < treshold) // Increase counter if pressed and less than treshold
                debounceCnt[keypadRow][i]++;
        }
        else
        {
             if (debounceCnt[keypadRow][i] > 0) // Decrease counter if not pressed
                debounceCnt[keypadRow][i]--;
        }

        if (debounceCnt[keypadRow][i] == treshold) // When counter reaches threshold then key pressed
        {
            if (pressedKey[keypadRow][i] == 0)
            {
                pressedKey[keypadRow][i] = true;
                if (xQueueSendToBack(uiEventQueue, &keyLookup[keypadRow][i], pdMS_TO_TICKS(15))!= pdPASS)
                {}
                // printf("Key [%d][%d] Pressed\n", keypadRow, i);
            }
               
        }
        else if (debounceCnt[keypadRow][i] == 0) // When counter reaches zero then key released
        {
            if (pressedKey[keypadRow][i] == 1)
            {
                pressedKey[keypadRow][i] = false;
                printf("Key [%d][%d] Released\n", keypadRow, i);
            }
            
        }
            
    }

    keypadRow++;
    if (keypadRow >= 10) 
    {
        // Pulse Reset to go back to Q0
        gpio_set_level(CD4017_RST, 1);
        esp_rom_delay_us(1); 
        gpio_set_level(CD4017_RST, 0);
        keypadRow = 0;
    } else 
    {
        // Pulse Clock to go to Q1, Q2, etc.
        gpio_set_level(CD4017_CLK, 1);
        esp_rom_delay_us(1);
        gpio_set_level(CD4017_CLK, 0);
    }
    
}


void print_deferred_task(void* args)
{
    touch_print_t evt;
    // char* tag = "print";
    char str[40];
    for (;;)
    {
        if (xQueueReceive(printQueue, &evt, portMAX_DELAY))
        {    
            evt.tick /= 240;
            switch (evt.evt)
            {
            case 0:
                printf("Time: %lu\tSync Bit Found\n", evt.tick);
                break;
            
            case 1:
                printf("Time: %lu\tStart word OK\n", evt.tick);
                break;
            
            case 2:
                printf("Time: %lu\tStart word BAD\n", evt.tick);
                break;
                        
            case 3:
                            
                printf("Time: %lu\tParity BAD\nbitCnt: %d\tdata: %s\n", evt.tick, evt.bitCnt, int32_to_char_bin(str, evt.data));
                break;
            
            case 4:
                printf("Start Metakom Read");
                
                break;
            
            case 5:
                printf("timeAvg: %lu\n", evt.timeAvg);
                break;

            case 6:                                
                printf("Time: %lu\tbitCnt: %d\tdata: %s\t dur: %lu\n", evt.tick, evt.bitCnt, int32_to_char_bin(str, evt.data), evt.duration);
                break;

            case 7:
                printf("Time: %lu\tstartCnt: %d\tstartWord: %s\n", evt.tick, evt.bitCnt, int32_to_char_bin(str, evt.data));
                break;

            case 8:
                printf("Metakom Data %lu, %s\n", evt.data, int32_to_char_bin(str, evt.data));
                break;

            default:
                break;
            }
        }

    }
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
    rfidInputIsrEvtQueue = xQueueCreate(20, sizeof(rfid_read_event_t));
    if (rfidInputIsrEvtQueue == NULL)
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
    touchInputIsrEvtQueue = xQueueCreate(10, sizeof(rfid_read_event_t));
    if (touchInputIsrEvtQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    printQueue = xQueueCreate(50, sizeof(touch_print_t));
    if (printQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

//-----------------------------------------------------------------------------
    gpio_pins_init();
//-----------------------------------------------------------------------------
//RMT TX RFID Transmit
    rmt_tx_channel_config_t rfid_tx_ch_config = {
        .gpio_num = COIL_OUTPUT_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1 MHz resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
    };    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&rfid_tx_ch_config, &rfid_tx_ch));
    rmt_copy_encoder_config_t copy_cfg = {};    
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &copy_enc));
//RMT TX Touch Memory
    rmt_tx_channel_config_t touch_tx_ch_config = {
        .gpio_num = METAKOM_TX,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1 MHz resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = true,
        
    };    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&touch_tx_ch_config, &touch_tx_ch));        
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &copy_enc));    
    // Enable and disable to set pin low
    err = rmt_enable(touch_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
    esp_err_t err = rmt_disable(touch_tx_ch);
    if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
        ESP_LOGE(TAG, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);

//-----------------------------------------------------------------------------
    // Create a task to process the deferred events.
    xTaskCreate(rfid_deferred_task, "rfid_deferred_task", 2048, NULL, 4, NULL);    
    xTaskCreate(print_deferred_task, "print_deferred_task", 2048, NULL, 3, NULL);      
//-----------------------------------------------------------------------------
    // Create semaphores
    // modeSwitchSem = xSemaphoreCreateBinary();
    scanSem = xSemaphoreCreateBinary();
    scanDoneSem = xSemaphoreCreateBinary();
    rfidDoneSem = xSemaphoreCreateBinary();
    scrollDeleteSem = xSemaphoreCreateBinary();
    drawMutex = xSemaphoreCreateMutex();    
    // configASSERT(modeSwitchSem != NULL);
    configASSERT(scanSem != NULL);
    configASSERT(scanDoneSem != NULL);
    configASSERT(rfidDoneSem != NULL);
    configASSERT(scrollDeleteSem != NULL);
    configASSERT(drawMutex != NULL);


    // Create the worker task
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
//-----------------------------------------------------------------------------
    // const esp_timer_create_args_t keypad_poll_timer_args = {
    //     .callback = &keypad_poll_callback,
    //     .name = "keypad_poll",                    
    //     };
    // err = esp_timer_create(&keypad_poll_timer_args, &keypad_poll_handler);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to create confirmation timer: %s", esp_err_to_name(err));
    //     return;
    // }
    // esp_timer_start_periodic(keypad_poll_handler, 555);

//-----------------------------------------------------------------------------
    const esp_timer_create_args_t confirmation_timer_args = {
        .callback = &confirmation_timer_callback,
        .name = "keypad_conf_timer",                    
        };
    err = esp_timer_create(&confirmation_timer_args, &confirmation_timer_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create confirmation timer: %s", esp_err_to_name(err));
        return;
    }

//-----------------------------------------------------------------------------
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

    // xTaskCreate(tag_tx_cycle_callback, "tag_tx_cycle_callback", 2048, NULL, 0, &rfidAutoTxHandler);
    // vTaskSuspend(rfidAutoTxHandler);    
    
}
