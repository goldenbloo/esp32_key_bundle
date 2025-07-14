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

menu_t resolveLocationMenu = {
    .menuId = REWRITE_MATCH_LOC_PROMPT,    
    .startPosY = 8,    

    .draw_func = resolve_location_menu_draw,
    .enter_func = resolve_location_menu_enter,
    .event_handler_func = resolve_location_menu_handle,
    .exit_func = resolve_location_menu_exit,
    .back_handler_func = go_to_main_menu, 
};


void resolve_location_menu_enter()
{
    resolveLocationMenu.status = EVT_ON_ENTRY;   
    bestLocsNum = find_best_locations_from_scan(ap_records, ap_count, bestLocs, false);
    resolveLocationMenu.selectedOption = NOT_SELECTED;
    if (bestLocsNum == 0) 
    {
        resolveLocationMenu.status = EVT_NO_MATCH;
        uint32_t event = EVT_NEXT_MENU;
        xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
    }
    
}

menu_t* resolve_location_menu_handle(int32_t event)
{
    if (resolveLocationMenu.status == EVT_ON_ENTRY) resolveLocationMenu.status = 0;
    if (bestLocsNum > 0) // Locations are found
    {
        switch (event)
        {
        case KEY_LEFT:
            resolveLocationMenu.selectedOption = SAVE_NEW_OPTION;
            resolveLocationMenu.status = 0;
            break;

        case KEY_RIGHT:
            resolveLocationMenu.selectedOption = OVERWRITE_OPTION;
            resolveLocationMenu.status = 0;
            break;

        case KEY_ENTER:
            switch (resolveLocationMenu.selectedOption)
            {
            case OVERWRITE_OPTION:
                memcpy(bestLocs[0].tag, currentTagArray, sizeof(currentTagArray));
                overwrite_location(&bestLocs[0]);
                scroll_task_stop();
                resolveLocationMenu.status = EVT_OVERWRITE_TAG;                
                display_delay_cb_arg = KEY_BACK;
                esp_timer_start_once(display_delay_timer_handle, 3000 * 1000ULL);
                break;

            case SAVE_NEW_OPTION:
                uint32_t evt = EVT_NEXT_MENU;
                xQueueSendToBack(uiEventQueue, &evt, pdMS_TO_TICKS(15));
                break;
            }
            break;
        
        case EVT_NEXT_MENU:
        if (resolveLocationMenu.nextMenu == NULL)
            return &saveTagMenu;
        else
            return resolveLocationMenu.nextMenu;
        break;
        }
    }
    else // No locations found
    {
        if (resolveLocationMenu.nextMenu == NULL)
            return &saveTagMenu;
        else
            return resolveLocationMenu.nextMenu;
    }

    return NULL;
}

void resolve_location_menu_exit()
{
    // ESP_LOGI("resolve_exit", "Stop draw task");
    esp_timer_stop(display_delay_timer_handle);
    scroll_task_stop();
    // if (scrollTaskHandle != NULL)
    //     vTaskDelete(scrollTaskHandle);
}

void resolve_location_menu_draw()
{
    const char* TAG = "resolve_draw";
    const char* matchFoundStr = "Location match found!";
    const char* matchPromptStr1 = "Save new location or";
    const char* matchPromptStr2 = "overwrite old tag?";
    const char* overwriteSuccStr1 = "Overwrite";
    const char* overwriteSuccStr2 = "Successful!!!";

    
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    int16_t charHeight = u8g2_GetMaxCharHeight(&u8g2) - 1;
    // int16_t charWidth = u8g2_GetMaxCharWidth(&u8g2);
    switch (resolveLocationMenu.status)
    {
    case EVT_OVERWRITE_TAG:
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr);
        u8g2_DrawUTF8(&u8g2, (u8g2_GetDisplayWidth(&u8g2) / 2) - (u8g2_GetStrWidth(&u8g2, (overwriteSuccStr1)) / 2),
                         u8g2_GetDisplayHeight(&u8g2)/2 - u8g2_GetMaxCharHeight(&u8g2),
                        overwriteSuccStr1);
        u8g2_DrawUTF8(&u8g2, (u8g2_GetDisplayWidth(&u8g2) / 2) - (u8g2_GetStrWidth(&u8g2, (overwriteSuccStr2)) / 2),
                         u8g2_GetDisplayHeight(&u8g2)/2,
                        overwriteSuccStr2);

        break;

    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        int16_t posY = resolveLocationMenu.startPosY + 1;
        u8g2_DrawUTF8(&u8g2, 0, posY, matchFoundStr);
        u8g2_SetDrawColor(&u8g2, 2);
        u8g2_DrawBox(&u8g2, 0, posY, u8g2_GetUTF8Width(&u8g2, matchFoundStr), charHeight);
        u8g2_SetDrawColor(&u8g2, 1);
        // u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
        // charHeight = u8g2_GetMaxCharHeight(&u8g2);
        posY += charHeight + 3;
        u8g2_DrawHLine(&u8g2, 0, posY - 1, 128);
        u8g2_DrawHLine(&u8g2, 0, posY + charHeight + 1, 128);
        int16_t posX = (u8g2_GetDisplayWidth(&u8g2) / 2) - (u8g2_GetStrWidth(&u8g2, (bestLocs[0].name)) / 2);
        if (u8g2_GetUTF8Width(&u8g2, bestLocs[0].name) > u8g2_GetDisplayWidth(&u8g2))
        {
            scroll_data_t* pTaskData = (scroll_data_t*) malloc(sizeof(scroll_data_t));
            pTaskData->delayStartStopMs = 1000;
            pTaskData->delayScrollMs = 100;            
            pTaskData->string = bestLocs[0].name;
            pTaskData->strWidth = u8g2_GetUTF8Width(&u8g2, bestLocs[0].name);
            pTaskData->textX = 0;
            pTaskData->textY = posY;    
            pTaskData->bgBoxX = 0;
            pTaskData->bgBoxY = posY;       

            // ESP_LOGI(TAG, "createTask string: %s; strWidth: %d", sd.string, sd.strWidth);
            xTaskCreate(scroll_text_task, "scroll_task", 2048, pTaskData, 2, &scrollTaskHandle);
        }
        else
            u8g2_DrawUTF8(&u8g2, posX, posY, bestLocs[0].name);

        posY += charHeight + 3;
        u8g2_DrawUTF8(&u8g2, 0, posY, matchPromptStr1);
        posY += charHeight - 1;
        u8g2_DrawUTF8(&u8g2, 0, posY, matchPromptStr2);
        [[fallthrough]];
        // break;


        default:
            switch (resolveLocationMenu.selectedOption)
            {
            case SAVE_NEW_OPTION:
                ESP_LOGI(TAG, "Draw SAVE_OPTION");
                u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, NEW_rewrite_img);
                break;

            case OVERWRITE_OPTION:
                ESP_LOGI(TAG, "Draw CANCEL_OPTION");
                u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, NEW_rewrite_img);
                u8g2_SetDrawColor(&u8g2, 2);
                u8g2_DrawBox(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8);
                u8g2_SetDrawColor(&u8g2, 1);

                break;

            case NOT_SELECTED:
            ESP_LOGI(TAG, "Draw NOT_SELECTED");
                u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, new_rewrite_unchecked_img);
                break;

            default:
                break;
            }
            break;
        }
}
