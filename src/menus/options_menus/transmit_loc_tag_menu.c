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


menu_t transmitLocTagMenu = {
    .menuId = LOC_WIFI_APS_UPDATE_MENU,    
    .startPosY = 8,
    .draw_func = transmit_loc_tag_menu_draw,
    .event_handler_func = transmit_loc_tag_menu_handle,
    .enter_func = transmit_loc_tag_menu_enter,
    .exit_func = transmit_loc_tag_menu_exit,   
};

void transmit_loc_tag_menu_enter()
{   
    transmitLocTagMenu.status = EVT_ON_ENTRY;
    transmitLocTagMenu.selectedOption = NOT_SELECTED;
    rfid_disable_rx_tx_tag();
    uint64_t rawTag = rfid_arr_tag_to_raw_bitstream(currentLoc.keyData.rfid.id);
    rfid_enable_tx_raw_tag(rawTag);
}

void transmit_loc_tag_menu_exit()
{
    rfid_disable_rx_tx_tag();
}

menu_t* transmit_loc_tag_menu_handle(ui_event_e event)
{   
    transmitLocTagMenu.status = EVT_NONE;
    return NULL;
}

void transmit_loc_tag_menu_draw()
{
    if (transmitLocTagMenu.status == EVT_ON_ENTRY)
    {
        const char *tagTransmitStr = "Transmitting tag:";
        const char *locNameStr = "Location:";
        int8_t charHeight = u8g2_GetMaxCharHeight(&u8g2);

        u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

        uint16_t posY = transmitLocTagMenu.startPosY + 2;

        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, tagTransmitStr) / 2, posY, tagTransmitStr);

        posY += charHeight;
        char tagStr[17] = {0};
        sprintf(tagStr, "0x%010llX", currentLoc.keyData.value);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, tagStr) / 2, posY, tagStr);

        posY += charHeight;
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locNameStr) / 2, posY, locNameStr);

        posY += charHeight;
        if (u8g2_GetUTF8Width(&u8g2, currentLoc.name) > u8g2_GetDisplayWidth(&u8g2))
        {
            scroll_data_t *pTaskData = (scroll_data_t *)malloc(sizeof(scroll_data_t));
            pTaskData->delayStartStopMs = 1000;
            pTaskData->delayScrollMs = 100;
            pTaskData->string = currentLoc.name;
            pTaskData->strWidth = u8g2_GetUTF8Width(&u8g2, currentLoc.name);
            pTaskData->textX = 0;
            pTaskData->textY = posY;
            pTaskData->bgBoxX = 0;
            pTaskData->bgBoxY = posY;
            pTaskData->invert = false;

            ESP_LOGI("transmit_loc_menu", "createTask string: %s; strWidth: %d", pTaskData->string, pTaskData->strWidth);            
            xTaskCreate(scroll_text_task, "scroll_task", 2048, pTaskData, 2, &scrollTaskHandle);
        }
        else
            u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, currentLoc.name) / 2, posY, currentLoc.name);
    }
}
