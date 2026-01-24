#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rfid.h"
#include "touch.h"
#include "menus.h"

#include "littlefs_records.h"
#include "globals_menus.h"

TaskHandle_t touchReadTask = NULL;

menu_t scanTagMenu = {
    .menuId = TAG_SCAN,    
    .startPosY = 8,    
    .draw_func = scan_tag_menu_draw,
    .event_handler_func = scan_tag_menu_handle,
    .enter_func = scan_tag_menu_enter,
    .exit_func = scan_tag_menu_exit,
    .back_handler_func = go_to_main_menu,
};

void scan_tag_menu_enter()
{
    scanTagMenu.status = EVT_ON_ENTRY;
    rfid_enable_rx_tag();
    touch_rx_enable();
    if (touchReadTask == NULL)
        xTaskCreate(touch_read_task, "touch_read_task", 2048, NULL, 4, &touchReadTask);
}

menu_t *scan_tag_menu_handle(ui_event_e event)
{
    if (scanTagMenu.status == EVT_ON_ENTRY)
        scanTagMenu.status = EVT_NONE;
    const char *TAG = "scan_tag_handle";
    switch (event)
    {
    case EVT_KEY_SCAN_DONE:
        scanTagMenu.status = event; // Send event data to dislapy callback
        if (touchReadTask != NULL)
            vTaskDelete(touchReadTask); // Delete task
        touchReadTask = NULL;
        // Delay timer to show access points
        display_delay_cb_arg = EVT_NEXT_MENU;
        esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 4000 * 1000ULL);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));
        break;
    
    case EVT_NEXT_MENU:
    if ( scanTagMenu.nextMenu == NULL)
    {
        scanWifiMenu.nextMenu = &resolveLocationMenu; // Set next menu in wifi scan menu
        return &scanWifiMenu;
    }
    else
        return scanTagMenu.nextMenu;
    break;

    default:
        break;
    }    
    return NULL;
}

void scan_tag_menu_exit()
{    
    rfid_disable_rx_tx();
    touch_rx_disable();
    esp_timer_stop(display_delay_timer_handle);
    if (touchReadTask)
    {
        vTaskDelete(touchReadTask);
        touchReadTask = NULL;
    }
}

void scan_tag_menu_draw()
{
    const char *keyScanningStr = "Scanning tag...";
    const char *doneStr = "Scan succesfull";
    uint16_t keyScanStrY = u8g2_GetDisplayHeight(&u8g2) / 2 - 10;
    uint16_t doneStrY = u8g2_GetDisplayHeight(&u8g2) / 2 - 20;
    uint16_t typeStrY = u8g2_GetDisplayHeight(&u8g2) / 2 - 5;
    uint16_t keyStrY = u8g2_GetDisplayHeight(&u8g2) / 2 + 10;
 

    u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr);
    u8g2_ClearBuffer(&u8g2);
    // int startPosY = scanTagMenu.startPosY;
    if (scanTagMenu.status == EVT_KEY_SCAN_DONE)
    {
        char keyStr[17] = {0};
        const char* formatStr = "0x%010llX";
        const char* typeStr = get_key_type_string(currentKeyType); // Default
        switch (currentKeyType){
        case KEY_TYPE_EM4100_MANCHESTER_64:            
            formatStr = "0x%010llX";
            break;
        case KEY_TYPE_KT2:
             formatStr = "0x%08llX";
            break;
        case KEY_TYPE_IBUTTON:            
            formatStr = "0x%016llX";
            break;
        default:            
         break;
        }

        sprintf(keyStr, formatStr, currentKeyData.value);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetStrWidth(&u8g2, doneStr) / 2,
                      doneStrY, doneStr);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetStrWidth(&u8g2, typeStr) / 2,
                      typeStrY, typeStr);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetStrWidth(&u8g2, keyStr) / 2,
                      keyStrY, keyStr);

    }
    else if (scanTagMenu.status == EVT_ON_ENTRY)
    {
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetStrWidth(&u8g2, keyScanningStr) / 2,
                      keyScanStrY, keyScanningStr);
    }
}