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


menu_t deleteLocMenu = {
    .menuId = LOC_DELETE_MENU,    
    .startPosY = 8,
    .draw_func = delete_loc_menu_draw,
    .event_handler_func = delete_loc_menu_handle,
    .enter_func = delete_loc_menu_enter,
    .back_handler_func = go_to_main_menu,
   
};

void delete_loc_menu_enter()
{   
    deleteLocMenu.status = EVT_ON_ENTRY;
    deleteLocMenu.selectedOption = NOT_SELECTED;
}

menu_t* delete_loc_menu_handle(ui_event_e event)
{
    if (deleteLocMenu.status == EVT_ON_ENTRY) deleteLocMenu.status = EVT_NONE;

    if (deleteLocMenu.status != EVT_DELETE_SUCCESS && deleteLocMenu.status != EVT_DELETE_FAIL)
        switch (event)
        {

        case KEY_LEFT:
            deleteLocMenu.selectedOption = DELETE_OPTION;
            deleteLocMenu.status = EVT_NONE;
            break;

        case KEY_RIGHT:
            deleteLocMenu.selectedOption = CANCEL_OPTION;
            deleteLocMenu.status = EVT_NONE;
            break;

        case KEY_ENTER:
            switch (deleteLocMenu.selectedOption)
            {
            case DELETE_OPTION:
                

                if (delete_location(currentLoc.id))
                    deleteLocMenu.status = EVT_DELETE_SUCCESS;
                else
                    deleteLocMenu.status = EVT_DELETE_FAIL;
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

void delete_loc_menu_draw()
{
    // const char* TAG = "savetag";
    const char* deleteLocPromptStr = "Delete Location?";
    const char *locDeletedStr = "Location Deleted";
    const char *locDeleteFailedStr = "ERROR: Delete Failed";
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

    uint16_t locPromptPosY = deleteLocMenu.startPosY + 2;    

    switch (deleteLocMenu.status)
    {
    case EVT_DELETE_SUCCESS:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locDeletedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locDeletedStr);
        break;

    case EVT_DELETE_FAIL:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locDeleteFailedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locDeleteFailedStr);
        break;

    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, 0, locPromptPosY, deleteLocPromptStr);
        [[fallthrough]];

    default:

        switch (deleteLocMenu.selectedOption)
        {
        case DELETE_OPTION:
            // ESP_LOGI(TAG, "Draw SAVE_OPTION");
            u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8 ,128,8,DELETE_cancel_img);
                break;

        case CANCEL_OPTION:
            // ESP_LOGI(TAG, "Draw CANCEL_OPTION");
            
            u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8 ,128,8,DELETE_cancel_img);
            u8g2_SetDrawColor(&u8g2, 2);
            u8g2_DrawBox(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2)-8, 128, 8);
            u8g2_SetDrawColor(&u8g2, 1);

            break;
        
        case NOT_SELECTED:    
        // ESP_LOGI(TAG, "Draw NOT_SELECTED");
        u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, delete_cancel_unchecked_img);
            break;
        
        default:
            break;
        }
        break;
    }

}

