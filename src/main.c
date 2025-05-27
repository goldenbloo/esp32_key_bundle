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
#include "ssd1306.h"
// #include "font8x8_basic.h"
#include "esp_littlefs.h"
#include "rfid.h"
#include "menus.h"
#include <esp_timer.h>

#define ESP_INTR_FLAG_DEFAULT   0
#define READ_MODE               0
#define TRANSMIT_MODE           1
#define RMT_SIZE                64
#define DATA_FILE_PATH "/littlefs/locations.dat"






static const char *TAG = "ISR_offload";
static const char *TAG2 = "button_offload";
static const char *TAG3 = "wifi_scan";
static const char *TAG4 = "littleFS";
static const char *TAG5 = "SSD1306";

// gptimer_handle_t signalTimer = NULL;
rmt_channel_handle_t tx_chan = NULL;
rmt_encoder_handle_t copy_enc;
rmt_transmit_config_t trans_config  = {
        .loop_count = -1,
        .flags.eot_level = false,
        .flags.queue_nonblocking = true,
    };
rmt_symbol_word_t pulse_pattern[RMT_SIZE];
SemaphoreHandle_t modeSwitchSem, scanSem, scanDoneSem, rfidDoneSem;
SSD1306_t dev;
uint64_t tag1 = 0xff8e2001a5761700, tag2 = 0x900b7c28d, currentTag;
QueueHandle_t uartQueue, actionQueue;


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
                // if (last_tag != long_tag)
                // {
                //     last_tag = long_tag;
                //     sprintf(str, "0x%010" PRIX64, long_tag);
                //     // ssd1306_display_text(&dev, 0, str, 12, 0);
                //     ESP_LOGI(TAG, "tag: %#llx, idx: %ld", long_tag, evt.idx);
                //     ESP_LOGI(TAG, "str: %s", int_to_char_bin(str,evt.buf));
                // }                
                xSemaphoreGive(rfidDoneSem);
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

            if (mode == READ_MODE)
            {
                // Trying to disable RMT
                esp_err_t err = rmt_disable(tx_chan);
                if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
                    ESP_LOGE(TAG2, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
                // Set rmt symbol array to transmit 125 khz signal
                ESP_ERROR_CHECK(rmt_enable(tx_chan));
                for (uint8_t i = 0; i < 64; i++)
                {
                    pulse_pattern[i].duration0 = 4;
                    pulse_pattern[i].duration1 = 4;
                    pulse_pattern[i].level0 = 1;
                    pulse_pattern[i].level1 = 0;
                }
                // Start RMT TX with 125 khz signal
                ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, pulse_pattern, sizeof(pulse_pattern), &trans_config));
                ESP_LOGI(TAG2, "rmt tx carrier");
                // Enable coil VCC
                gpio_set_level(COIL_VCC_PIN, 1);
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
                // ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_chan, portMAX_DELAY));
                // Trying to disable RMT
                esp_err_t err = rmt_disable(tx_chan);
                // Set raw tag into rmt symbol array
                raw_tag_to_rmt(pulse_pattern, tag1);
                // Start RMT TX
                if (err != ESP_ERR_INVALID_STATE && err != ESP_OK)
                    ESP_LOGE(TAG2, "Error occurred: %s (0x%x)", esp_err_to_name(err), err);
                ESP_ERROR_CHECK(rmt_enable(tx_chan));
                ESP_ERROR_CHECK(rmt_transmit(tx_chan, copy_enc, pulse_pattern, sizeof(pulse_pattern), &trans_config));
                ESP_LOGI(TAG2, "rmt tx tag");
                gpio_set_level(LED_PIN, 0);
            }
            // list_test(&dev);
        }
    }
}

static void scan_wifi_and_tag(void *params)
{
     static const uint8_t save_cancel_image[] = {// 'Untitled-1', 128x8px
0xff, 0xff, 0x87, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x80, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x07, 0x00, 
0xff, 0xff, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x06, 0x60, 0x00, 0x00, 0x00, 0x03, 0x00, 
0xff, 0xff, 0x1f, 0x87, 0x33, 0x87, 0xff, 0xfe, 0x80, 0x0c, 0x07, 0x8f, 0x87, 0x87, 0x83, 0x00, 
0xff, 0xff, 0x8f, 0xf3, 0x33, 0x33, 0xff, 0xff, 0x00, 0x0c, 0x00, 0xcc, 0xcc, 0xcc, 0xc3, 0x00, 
0xff, 0xff, 0xe3, 0x83, 0x33, 0x03, 0xff, 0xfe, 0x80, 0x0c, 0x07, 0xcc, 0xcc, 0x0f, 0xc3, 0x00, 
0xff, 0xff, 0x33, 0x33, 0x87, 0x3f, 0xff, 0xff, 0x00, 0x06, 0x6c, 0xcc, 0xcc, 0xcc, 0x03, 0x00, 
0xff, 0xff, 0x87, 0x89, 0xcf, 0x87, 0xff, 0xfe, 0x80, 0x03, 0xc7, 0x6c, 0xc7, 0x87, 0x87, 0x80, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (;;)
    {
        if (xSemaphoreTake(scanSem, portMAX_DELAY) == pdTRUE)
        {
            wifi_scan_config_t scan_cfg = {
                .ssid = NULL,       // scan for all SSIDs
                .bssid = NULL,      // scan for all BSSIDs
                .channel = 0,       // 0 = scan all channels
                .show_hidden = true // include hidden SSIDs
            };
            esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_wifi_scan_start: %s", esp_err_to_name(err));
                continue;
            }
            //Wait for wifi scan
            if (xSemaphoreTake(scanDoneSem, pdMS_TO_TICKS(10000)) == pdTRUE)
            {
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);
                wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
                esp_wifi_scan_get_ap_records(&ap_count, ap_records);
                ESP_LOGI(TAG, "Total APs scanned = %d", ap_count);
                for (int i = 0; i < ap_count; i++)
                {
                    ESP_LOGI(TAG3, "SSID \t\t%s", ap_records[i].ssid);
                    ESP_LOGI(TAG3, "BSSID \t\t%02x:%02x:%02x:%02x:%02x:%02x", ap_records[i].bssid[0],
                             ap_records[i].bssid[1],
                             ap_records[i].bssid[2],
                             ap_records[i].bssid[3],
                             ap_records[i].bssid[4],
                             ap_records[i].bssid[5]);
                    ESP_LOGI(TAG3, "RSSI \t\t%d", ap_records[i].rssi);
                    ESP_LOGI(TAG3, "Authmode \t%d", ap_records[i].authmode);
                }
                ssd1306_bitmaps(&dev,0,55,save_cancel_image,128,8,false);
                //Display wifi records. Max 5 lines
                display_wifi_aps(&dev, ap_records, ap_count);
                ssd1306_display_text(&dev,5,"Reading tag ", 12 , false);
                //Read and display RFID tag
                if (xSemaphoreTake(rfidDoneSem, pdMS_TO_TICKS(30000)) == pdTRUE)
                {
                    char str[16];
                    sprintf(str, "0x%010" PRIX64, currentTag);
                    ssd1306_clear_line(&dev,5,false);
                    ssd1306_clear_line(&dev,6,false);                                       
                    ssd1306_display_text(&dev, 5, "Success", 7, 0);
                    ssd1306_display_text(&dev, 6, str, 12, 0);                     
                }
                else
                {
                    ssd1306_clear_line(&dev,5,false);
                    ssd1306_display_text(&dev, 5, "Read timeout", 12, 0);
                }
                // Wait for buttons
                char action;
                for (;;)
                {
                    if (xQueueReceive(actionQueue, &action,pdMS_TO_TICKS(30000)) == pdTRUE)
                    {
                        switch (action)
                        {
                        case ACTION_LEFT:
                            ssd1306_bitmaps(&dev,0,55,save_cancel_image,128,8,false); // Save
                            break;

                        case ACTION_RIGHT:
                            ssd1306_bitmaps(&dev,0,55,save_cancel_image,128,8,true); // Cancel4ad
                            break;

                        case ACTION_ENTER:
                            goto exit1;
                            break;
                        
                        default:
                            break;
                        }    
                    }
                }
exit1:          display_loc_save(&dev, actionQueue);
                free(ap_records);
            }
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        xSemaphoreGive(scanDoneSem);
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
#if CONFIG_I2C_INTERFACE
    ESP_LOGI(TAG5, "INTERFACE is i2c");
    ESP_LOGI(TAG5, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
    ESP_LOGI(TAG5, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
    ESP_LOGI(TAG5, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);

#endif // CONFIG_I2C_INTERFACE
#if CONFIG_FLIP
    dev._flip = true;
    ESP_LOGW(TAG5, "Flip upside down");
#endif
#if CONFIG_SSD1306_128x64
    ESP_LOGI(TAG5, "Panel is 128x64");
    ssd1306_init(&dev, 128, 64);
#endif
    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);
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

void littlefs_init()
{
        esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG4, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG4, "Failed to find LittleFS partition");
        else
            ESP_LOGE(TAG4, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
        ESP_LOGE(TAG4, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    else
        ESP_LOGI(TAG4, "Partition size: total: %d, used: %d", total, used);

}




void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    char msg[17] = {0};
    char textBuffer[17] = {0};
    for(;;) {
        // Waiting for UART event.
        if(xQueueReceive(uartQueue, (void * )&event, (TickType_t)portMAX_DELAY)) {            
            switch(event.type) {
                case UART_DATA:
                    if (uart_read_bytes(UART_NUM_0, msg, event.size, portMAX_DELAY) > -1)
                    {
                        char action = msg[0];
                        if (action >= 0x30 && action <= 0x39)
                        {
                            action -= 0x30;
                            keypad_button_press(action, textBuffer, sizeof(textBuffer), &dev);
                        }
                        else if (action == 0x08)
                        {
                            action = 10;
                            keypad_button_press(action, textBuffer, sizeof(textBuffer), &dev);
                        }

                        // char action = 0;
                        // switch (msg[0])
                        // {
                        // case 'w': // UP
                        //     action = ACTION_UP;
                        //     break;
                        // case 's': // DOWN
                        //     action = ACTION_DOWN;
                        //     break;
                        // case 'a': // LEFT
                        //     action = ACTION_LEFT;
                        //     break;
                        // case 'd': // RIGHT
                        //     action = ACTION_RIGHT;
                        //     break;
                        // case 0xD: // RIGHT
                        //     action = ACTION_ENTER;
                        //     break;
                        
                        // default:
                        //     break;
                        // }
                        // if (xQueueSendToBack(actionQueue, &action, pdMS_TO_TICKS(15) != pdPASS))
                        // {
                        //     ESP_LOGI(localTAG,"Can't send action to a queue");
                        // }
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

void textbox_dispay(void *pvParameters)
{
    for (int i = 0; i < 4; i++)
    ssd1306_display_text_box1(&dev, 1, 0, "A very long message that's not fits", 10, 35, false, 5);
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
    ssd1306_display_text(&dev, 0, "hello", 5, false);
    //-----------------------------------------------------

//  esp_timer_create_args_t confirmation_timer_args = {
//             .callback = &confirmation_timer_callback,
//             .name = "t9_confirmation_timer"
//     };
//     esp_err_t err = esp_timer_create(&confirmation_timer_args, &confirmation_timer_handle);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to create confirmation timer: %s", esp_err_to_name(err));
//         return;
//     }
//-----------------------------------------------------------------------------
    // Create the queue. 
    inputIsrEvtQueue = xQueueCreate(50, sizeof(rfid_read_event_t));
    if (inputIsrEvtQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    actionQueue = xQueueCreate(10, sizeof(char));
    if (actionQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create action queue");
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
    xTaskCreate(rfid_deferred_task, "rfid_deferred_task", 3048, NULL, 2, NULL);    
//-----------------------------------------------------------------------------
    // Create semaphores
    modeSwitchSem = xSemaphoreCreateBinary();
    scanSem = xSemaphoreCreateBinary();
    scanDoneSem = xSemaphoreCreateBinary();
    rfidDoneSem = xSemaphoreCreateBinary();
    configASSERT(modeSwitchSem != NULL);
    configASSERT(scanSem != NULL);
    configASSERT(scanDoneSem != NULL);
    configASSERT(rfidDoneSem != NULL);

    // Create the worker task
    xTaskCreate(mode_switch_task, "mode_switch", 2048, 0, 0, NULL);
    xTaskCreate(scan_wifi_and_tag, "scan_wifi_&_tag", 4096, 0, 1, NULL);
    // ESP_ERROR_CHECK(rmt_disable(tx_chan));
    xSemaphoreGive(modeSwitchSem);
//-----------------------------------------------------------------------------
    //LittleFS
    littlefs_init();
//-----------------------------------------------------------------------------
    uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};
    uart_param_config(UART_NUM_0, &uart_config);
    const int uart_buffer_size = (1024 * 2);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, uart_buffer_size, 0, 10, &uartQueue, 0);
    
    xTaskCreate(uart_event_task, "uart_receive_task", 2048, NULL, 10, NULL);
    // xTaskCreate(textbox_dispay, "tbt", 2048, NULL, 1, NULL);


    const esp_timer_create_args_t confirmation_timer_args = {
        .callback = &confirmation_timer_callback,
        .name = "keypad_timer",
        .arg = &dev,            
        };
    esp_err_t err = esp_timer_create(&confirmation_timer_args, &confirmation_timer_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create confirmation timer: %s", esp_err_to_name(err));
        return;
    }
}