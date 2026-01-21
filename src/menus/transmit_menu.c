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

menu_listbox_t transmitMenuListBox = {
    .selectedRow = 0,
    .list = locNameList,
    .maxRows = 6,
};


menu_t transmitMenu = {
    .menuId = TRANSMIT_TAG_MENU,
    .listBox = &transmitMenuListBox,
    .startPosY = 8,

    .draw_func = transmit_menu_draw,
    .enter_func = transmit_menu_enter,
    .event_handler_func = transmit_menu_handle,
    .exit_func = transmit_menu_exit,    
    .back_handler_func = go_to_main_menu,
    
};

void transmit_menu_enter()
{
    const char *TAG = "transmit_enter";
    
    transmitMenu.listBox->selectedRow = 0;
    transmitMenu.status = EVT_ON_ENTRY;    

    // for (int i = 0; i < ap_count; i++)
    // {
    //     ESP_LOGI(TAG, "SSID %s", ap_records[i].ssid);
    //     ESP_LOGI(TAG, "BSSID %02x:%02x:%02x:%02x:%02x:%02x", ap_records[i].bssid[0],
    //              ap_records[i].bssid[1],
    //              ap_records[i].bssid[2],
    //              ap_records[i].bssid[3],
    //              ap_records[i].bssid[4],
    //              ap_records[i].bssid[5]);
    //     ESP_LOGI(TAG, "RSSI %d", ap_records[i].rssi);
    // }   
    
    bestLocsNum = find_best_locations_from_scan(ap_records, ap_count, bestLocs, false);
    transmitMenu.listBox->listSize = bestLocsNum + 1;
    transmitMenu.listBox->list = locNameList;
    
    locNameList[0] = "Auto";
    for (int i = 0; i < bestLocsNum; i++)
    {
        ESP_LOGI(TAG, "Id: %ld\nName: %s\nTag: 0x%010llX", bestLocs[i].id, bestLocs[i].name, bestLocs[i].keyData.value);
        locNameList[i + 1] = bestLocs[i].name;
    }

    if (rfidAutoTxHandler == NULL)
        xTaskCreate(tag_tx_cycle_callback, "tag_tx_cycle_callback", 2048, NULL, 0, &rfidAutoTxHandler);
}

menu_t* transmit_menu_handle(ui_event_e event)
{
    if (transmitMenu.status == EVT_ON_ENTRY) transmitMenu.status = EVT_NONE;

    if (bestLocsNum == 0)
    {
        transmitMenu.status = EVT_NO_MATCH;
        display_delay_cb_arg = KEY_BACK;
        esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
    }
    else
    {
        list_event_handle(&transmitMenu, event);
        if (transmitMenu.listBox->selectedRow == 0)        
            vTaskResume(rfidAutoTxHandler);
        
        else
        {
            vTaskSuspend(rfidAutoTxHandler);
            int8_t idx = transmitMenu.listBox->selectedRow - 1;
            if (idx > sizeof(bestLocs))
                ESP_LOGE("transmit_menu_handle", "idx > sizeof(bestLocs)");
            uint64_t rawTag = rfid_arr_tag_to_raw_bitstream(bestLocs[idx].keyData.rfid.id);
            rfid_enable_tx_raw_tag(rawTag);
        }
    }
    return NULL;
}

void transmit_menu_exit()
{
    // vTaskSuspend(rfidAutoTxHandler);
    if (rfidAutoTxHandler != NULL)
    {
        vTaskDelete(rfidAutoTxHandler);
        rfidAutoTxHandler = NULL;
    }
    rfid_disable_rx_tx_tag();
    esp_timer_stop(display_delay_timer_handle);
}

void transmit_menu_draw()
{
    const char* noMatchStr = "Tag not found";
    
    if (transmitMenu.status == EVT_NO_MATCH)
    {        
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr  );
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetStrWidth(&u8g2, noMatchStr) / 2,
                      u8g2_GetDisplayHeight(&u8g2) / 2 - 10, noMatchStr);
    }
    else        
        display_list(&transmitMenu);
    
}
