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


menu_t updateTagMenu = {
    .menuId = LOC_TAG_UPDATE_MENU,    
    .startPosY = 8,
    .draw_func = update_tag_menu_draw,
    .event_handler_func = update_tag_menu_handle,
    .enter_func = update_tag_menu_enter,
    .back_handler_func = go_to_loc_options_menu,
   
};

void update_tag_menu_enter()
{   
    updateTagMenu.status = EVT_ON_ENTRY;
    updateTagMenu.selectedOption = NOT_SELECTED;
}

menu_t* update_tag_menu_handle(ui_event_e event)
{
    if (updateTagMenu.status == EVT_ON_ENTRY) updateTagMenu.status = EVT_NONE;

    if (updateTagMenu.status != EVT_SAVE_SUCCESS && updateTagMenu.status != EVT_SAVE_FAIL)
    {
        switch (event)
        {

        case KEY_LEFT:
            updateTagMenu.selectedOption = SAVE_OPTION;
            updateTagMenu.status = EVT_NONE;
            break;

        case KEY_RIGHT:
            updateTagMenu.selectedOption = CANCEL_OPTION;
            updateTagMenu.status = EVT_NONE;
            break;

        case KEY_ENTER:
            switch (updateTagMenu.selectedOption)
            {
            case SAVE_OPTION:

                memcpy(&currentLoc.keyData.value, &currentKeyData.value, sizeof(currentLoc.keyData));
                if (write_location(&currentLoc))
                    updateTagMenu.status = EVT_SAVE_SUCCESS;
                else
                    updateTagMenu.status = EVT_SAVE_FAIL;
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
    }
    return NULL;
}

void update_tag_menu_draw()
{
    // const char* TAG = "savetag";
    const char* tagUpdatePromptStr = "Save new tag record?";
    const char *locTagSavedStr = "Location Tag Saved";
    const char *locSaveFailedStr = "ERROR: Save Failed";
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

    uint16_t locPromptPosY = updateTagMenu.startPosY + 2;    

    switch (updateTagMenu.status)
    {   

    case EVT_SAVE_SUCCESS:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locTagSavedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locTagSavedStr);
        break;

    case EVT_SAVE_FAIL:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locSaveFailedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locSaveFailedStr);
        break;
        
    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, 0, locPromptPosY, tagUpdatePromptStr);
        [[fallthrough]];

    default:       
    
        switch (updateTagMenu.selectedOption)
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
