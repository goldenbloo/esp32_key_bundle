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


menu_t updateWifiMenu = {
    .menuId = LOC_WIFI_APS_UPDATE_MENU,    
    .startPosY = 8,
    .draw_func = update_wifi_menu_draw,
    .event_handler_func = update_wifi_menu_handle,
    .enter_func = update_wifi_menu_enter,
    .back_handler_func = go_to_loc_options_menu,
   
};

void update_wifi_menu_enter()
{   
    updateWifiMenu.status = EVT_ON_ENTRY;
    updateWifiMenu.selectedOption = NOT_SELECTED;
}

menu_t* update_wifi_menu_handle(int32_t event)
{
    if (updateWifiMenu.status == EVT_ON_ENTRY) updateWifiMenu.status = 0;

    if (updateWifiMenu.status != EVT_SAVE_SUCCESS && updateWifiMenu.status != EVT_SAVE_FAIL)
        switch (event)
        {

        case KEY_LEFT:
            updateWifiMenu.selectedOption = SAVE_OPTION;
            updateWifiMenu.status = 0;
            break;

        case KEY_RIGHT:
            updateWifiMenu.selectedOption = CANCEL_OPTION;
            updateWifiMenu.status = 0;
            break;

        case KEY_ENTER:
            switch (updateWifiMenu.selectedOption)
            {
            case SAVE_OPTION:
                for (uint8_t i = 0; i < ap_count && i < BSSID_MAX; i++)
                {
                    memcpy(currentLoc.bssids[i], ap_records[i].bssid, sizeof(currentLoc.bssids[i]));
                    currentLoc.rssis[i] = ap_records[i].rssi;
                }
                for (uint8_t i = ap_count; i < BSSID_MAX; i++)
                {
                    memset(currentLoc.bssids[i], 0, sizeof(currentLoc.bssids[i]));
                    currentLoc.rssis[i] = 0;
                }

                if (write_location(&currentLoc))
                    updateWifiMenu.status = EVT_SAVE_SUCCESS;
                else
                    updateWifiMenu.status = EVT_SAVE_FAIL;
                display_delay_cb_arg = KEY_BACK;
                esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
                break;

            case CANCEL_OPTION:
                uint32_t backEvent = KEY_BACK;
                xQueueSendToBack(uiEventQueue, &backEvent, pdMS_TO_TICKS(15));
                break;

            default:
                break;
            }

            break;

        default:
            break;
        }
    return NULL;
}

void update_wifi_menu_draw()
{
    // const char* TAG = "savetag";
    const char* wifiUpdatePromptStr = "Save new AP records?";
    const char *locWifiSavedStr = "Location APs Saved";
    const char *locSaveFailedStr = "ERROR: Save Failed";
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

    uint16_t locPromptPosY = updateWifiMenu.startPosY + 2;    

    switch (updateWifiMenu.status)
    {
    case EVT_SAVE_SUCCESS:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locWifiSavedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locWifiSavedStr);
        break;

    case EVT_SAVE_FAIL:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locSaveFailedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locSaveFailedStr);
        break;

    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, 0, locPromptPosY, wifiUpdatePromptStr);
        [[fallthrough]];

    default:

        switch (updateWifiMenu.selectedOption)
        {
        case SAVE_OPTION:
            // ESP_LOGI(TAG, "Draw SAVE_OPTION");
            u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8 ,128,8,SAVE_cancel_img);
                break;

        case CANCEL_OPTION:
            // ESP_LOGI(TAG, "Draw CANCEL_OPTION");
            
            u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8 ,128,8,SAVE_cancel_img);
            u8g2_SetDrawColor(&u8g2, 2);
            u8g2_DrawBox(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2)-8, 128, 8);
            u8g2_SetDrawColor(&u8g2, 1);

            break;
        
        case NOT_SELECTED:    
        // ESP_LOGI(TAG, "Draw NOT_SELECTED");
        u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, save_cancel_unchecked_img);
            break;
        
        default:
            break;
        }
        break;
    }

}

