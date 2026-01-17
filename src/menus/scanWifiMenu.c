#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rfid.h"
#include "menus.h"

#include "littlefs_records.h"
#include "globals_menus.h"

menu_t scanWifiMenu = {
    .menuId = WIFI_SCAN,    
    .startPosY = 8,    
    .draw_func = scan_wifi_menu_draw,
    .enter_func = scan_wifi_menu_enter,
    .event_handler_func = scan_wifi_menu_handle,
    .exit_func = scan_wifi_menu_exit,
    .back_handler_func = go_to_main_menu, 

};

void scan_wifi_menu_enter()
{
    const char *TAG = "wifi_enter";
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,       // scan for all SSIDs
        .bssid = NULL,      // scan for all BSSIDs
        .channel = 0,       // 0 = scan all channels
        .show_hidden = true // include hidden SSIDs
    };   
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "esp_wifi_scan_start: %s", esp_err_to_name(err));
    scanWifiMenu.status = EVT_ON_ENTRY;    
}

menu_t* scan_wifi_menu_handle(ui_event_e event)
{
    const char *TAG = "wifi_handle";
    if (scanWifiMenu.status == EVT_ON_ENTRY) scanWifiMenu.status = EVT_NONE;
    switch (event)
    {
    case EVT_WIFI_SCAN_DONE:
        scanWifiMenu.status = event;
        esp_wifi_scan_get_ap_num(&ap_count);
        if (ap_count > BSSID_MAX)
            ap_count = BSSID_MAX;
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);

        if (ap_count == 0) 
        {
            display_delay_cb_arg = KEY_BACK;        
            scanWifiMenu.status = EVT_APS_NOT_FOUND;
        }
        else display_delay_cb_arg = EVT_NEXT_MENU;

         esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
            if (err != ESP_OK)
                ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));
        break;

    case EVT_NEXT_MENU:
        if (scanWifiMenu.nextMenu != NULL)
            return scanWifiMenu.nextMenu;
        else
            return &saveTagMenu;
        break;

    default:
        break;
    }

    return NULL;
}

void scan_wifi_menu_exit()
{
    esp_timer_stop(display_delay_timer_handle);
}

void scan_wifi_menu_draw()
{
    // const char* TAG_WIFI_SCAN = "wifi_scan";    
    const char* wifiScanningStr = "Scanning WiFi...";    
    const char* wifiNoAPs = "Wifi APs not found";   
    if (scanWifiMenu.status == EVT_WIFI_SCAN_DONE)
    {
        u8g2_ClearBuffer(&u8g2);
        display_wifi_aps(ap_records,ap_count, scanWifiMenu.startPosY);
        
    } 
    else if (scanWifiMenu.status == EVT_ON_ENTRY || scanWifiMenu.status == EVT_APS_NOT_FOUND) 
    {
        const char* msgStr;
        if (scanWifiMenu.status == EVT_ON_ENTRY) msgStr = wifiScanningStr;
        else if (scanWifiMenu.status == EVT_APS_NOT_FOUND) msgStr = wifiNoAPs;
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr  );    
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2)/2 - u8g2_GetStrWidth(&u8g2, msgStr)/2,
                        u8g2_GetDisplayHeight(&u8g2)/2 - 10, msgStr);
        // ssd1306_clear_screen(devPtr, false);          
        // ssd1306_display_text(devPtr, scanWifiMenu.startPosY, "Scanning WiFi  ", 16, false);
    }    
    
}