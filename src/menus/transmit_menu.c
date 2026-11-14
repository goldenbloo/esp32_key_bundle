#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rfid.h"
#include "menus.h"
#include "touch.h"
#include "owi.h"

#include "littlefs_records.h"
#include "globals_menus.h"
void key_tx_cycle_callback();

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
        ESP_LOGI(TAG, "Id: %ld\nName: %s\nType: %s\nTag: 0x%llX", bestLocs[i].id, bestLocs[i].name, get_key_type_string(bestLocs[i].keyType), bestLocs[i].keyData.value);
        locNameList[i + 1] = bestLocs[i].name;
    }   
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
        {
            if (cycleKeyTxHandler == NULL)
                xTaskCreate(key_tx_cycle_callback, "key_tx_cycle_callback", 2048, NULL, 0, &cycleKeyTxHandler);
        }
        else
        {
            if (cycleKeyTxHandler != NULL)
            {
                vTaskDelete(cycleKeyTxHandler);
                cycleKeyTxHandler = NULL;                
            }
            int8_t idx = transmitMenu.listBox->selectedRow - 1;
            if (idx > sizeof(bestLocs))
                ESP_LOGE("transmit_menu_handle", "idx > sizeof(bestLocs)");
            ESP_LOGI("tx menu handle", "Id: %ld\nName: %s\tType: %s\tTag: 0x%llX", bestLocs[idx].id, bestLocs[idx].name, get_key_type_string(bestLocs[idx].keyType), bestLocs[idx].keyData.value);

            rfid_disable_rx_tx();
            touch_tx_disable();
            owi_slave_disable();
            switch (bestLocs[idx].keyType)
            {
            case KEY_TYPE_EM4100_MANCHESTER_64:
                rfid_enable_tx_raw(rfid_arr_tag_to_raw_bitstream(bestLocs[idx].keyData.rfid.id));
                break;
            case KEY_TYPE_METAKOM:
                touch_tx_metakom(bestLocs[idx].keyData.kt2.id);
                break;
            case KEY_TYPE_IBUTTON:
                owi_slave_enable(bestLocs[idx].keyData.ds1990.rom);
                break;
            default:
                break;
            }
        }
    }
    return NULL;
}

void transmit_menu_exit()
{
    // vTaskSuspend(keyCycleTxHandler);
    if (cycleKeyTxHandler != NULL)
    {
        vTaskDelete(cycleKeyTxHandler);
        cycleKeyTxHandler = NULL;
    }
    rfid_disable_rx_tx();
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

void key_tx_cycle_callback()
{
    int8_t idx = 0;
    if (bestLocsNum <= 0) return;

    for (;;)
    {
        switch (bestLocs[idx].keyType)
        {
        case KEY_TYPE_EM4100_MANCHESTER_64:
           rfid_enable_tx_raw(rfid_arr_tag_to_raw_bitstream(bestLocs[idx].keyData.rfid.id));
           vTaskDelay(1000 / portTICK_PERIOD_MS);
           rfid_disable_rx_tx();
            break;

        case KEY_TYPE_METAKOM:
            touch_tx_metakom(bestLocs[idx].keyData.kt2.id);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            touch_tx_disable();            
            break;
        
        case KEY_TYPE_IBUTTON:
            owi_slave_enable(bestLocs[idx].keyData.ds1990.rom);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            owi_slave_disable();
            break;
        
        default:
            break;
        }
        
        // ESP_LOGI("tag_tx_cycle", "loc: %s\ttag: 0x%010llX", bestLocs[i].name, keyData);
        
        idx++;
        if (idx >= bestLocsNum)
            idx = 0;
    }
}