#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rfid.h"
#include "menus.h"
#include "ssd1306.h"
#include "littlefs_records.h"
#include "globals_menus.h"

menu_textbox_t saveTagMenuTextBox = {
    .textFieldBuffer = fieldBuffer,
    .textFieldBufferSize = FIELD_SIZE,    
};

menu_t saveTagMenu = {
    .menuId = SAVE_TAG_MENU,    
    .startPosY = 8,    
    .textBox = &saveTagMenuTextBox,
    .draw_func = save_tag_menu_draw,
    .event_handler_func = save_tag_menu_handle,
    .enter_func = save_tag_menu_enter,
    .back_handler_func = go_to_main_menu,
   
};

void save_tag_menu_enter()
{        
    if (saveTagMenu.textBox == NULL){
        ESP_LOGE("wifi_enter", "Textbox in saveTagMenu in NULL"); 
        return;
    }
    memset(saveTagMenu.textBox->textFieldBuffer, 0, saveTagMenu.textBox->textFieldBufferSize); 
    keypad.bufferPos = -1;   
    keypad.lastPressedButton = -1;
    keypad.displayCharWidth = 16;
    saveTagMenu.status = EVT_ON_ENTRY;
    saveTagMenu.selectedOption = NO_OPTION;
}

menu_t* save_tag_menu_handle(int32_t event)
{
    if (saveTagMenu.status == EVT_ON_ENTRY) saveTagMenu.status = 0;
    switch (event)
    {
    case EVT_KEYPAD_PRESS:
        saveTagMenu.status = EVT_KEYPAD_PRESS;
        break;

    case KEY_LEFT:
        saveTagMenu.selectedOption = SAVE_OPTION;
        saveTagMenu.status = 0;
        break;

    case KEY_RIGHT:
        saveTagMenu.selectedOption = CANCEL_OPTION;
        saveTagMenu.status = 0;
        break;

    case KEY_ENTER:
        switch (saveTagMenu.selectedOption)
        {
        case SAVE_OPTION:

            int32_t currentLocId = get_next_location_id();
            if (currentLocId >= 0)
            {
                location_t loc = {.id = currentLocId};
                for (int i = 0; i < ap_count && i < BSSID_MAX; i++)
                {
                    memcpy(loc.bssids[i], ap_records[i].bssid, sizeof(loc.bssids[i]));
                    loc.rssis[i] = ap_records[i].rssi;
                }
                for (int i = ap_count; i < BSSID_MAX; i++)
                {
                    memset(loc.bssids[i], 0, sizeof(loc.bssids[i]));
                    loc.rssis[i] = 0;
                }
                strncpy(loc.name, saveTagMenu.textBox->textFieldBuffer, sizeof(loc.name));

                memcpy(loc.tag, currentTagArray, sizeof(loc.tag));
                append_location(&loc);
            }            
            // TODO: Show save or error message
            // Fall through to CANCEL_OPTION to return to main menu         
            [[fallthrough]];  
            // break;

        case CANCEL_OPTION:
         uint32_t backEvent = KEY_BACK;
            xQueueSendToBack(uiEventQueue, &backEvent, pdMS_TO_TICKS(15));
            break;
        
        default:
            break;
        }

        break;

    default:
        if (event >= KEY_0 && event <= KEY_SHIFT)
        {
            keypad_button_press(event);
            saveTagMenu.status = EVT_KEYPAD_PRESS;            
        }
        break;
    }
    return NULL;
}

void save_tag_menu_draw()
{
    const char* TAG = "savetag";
    const char* locPromptStr = "Enter Location Name:";
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

    int locPromptPosY = saveTagMenu.startPosY + 2;
    int textFieldPosY = locPromptPosY + u8g2_GetMaxCharHeight(&u8g2) + 3;
    int textFieldPosX = 5;    
    int displayCharWidth = (u8g2_GetDisplayWidth(&u8g2) - textFieldPosX) / (u8g2_GetMaxCharWidth(&u8g2) + 1) - 2;
    const char* startPtr = keypad.bufferPos < displayCharWidth ? keypad.textBuffer : keypad.textBuffer + (keypad.bufferPos - displayCharWidth + 1);
    // ESP_LOGI(TAG,"")
    // u8g2_SetFontPosTop(&u8g2);
    switch (saveTagMenu.status)
    {
    
    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, 0, locPromptPosY, locPromptStr);
        // u8g2_DrawFrame(&u8g2, textFieldPosX-2, textFieldPosY, (u8g2_GetMaxCharWidth(&u8g2)+1) * displayCharWidth + 8, u8g2_GetMaxCharHeight(&u8g2) + 2);
        u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, save_cancel_unchecked_img);
        [[fallthrough]];

    case EVT_KEYPAD_PRESS:
        // ESP_LOGI("savetag", "displayCharWidth: %d", displayCharWidth);
        u8g2_SetDrawColor(&u8g2, 0); // 0 = background
        u8g2_DrawBox(&u8g2, 0, textFieldPosY, u8g2_GetDisplayWidth(&u8g2), u8g2_GetMaxCharHeight(&u8g2));
        u8g2_SetDrawColor(&u8g2, 1); // Reset to foreground for next text
        u8g2_DrawFrame(&u8g2, textFieldPosX - 2, textFieldPosY, (u8g2_GetMaxCharWidth(&u8g2) + 1) * displayCharWidth + 8, u8g2_GetMaxCharHeight(&u8g2) + 2);
        u8g2_DrawUTF8(&u8g2, textFieldPosX, textFieldPosY, startPtr);
        break;

    default:        
        // int paddingH = 0;
        // int paddingV = 1;        
        switch (saveTagMenu.selectedOption)
        {
        case SAVE_OPTION:
            ESP_LOGI(TAG, "Draw SAVE_OPTION");
            u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8 ,128,8,SAVE_cancel_img);
                break;

        case CANCEL_OPTION:
            ESP_LOGI(TAG, "Draw CANCEL_OPTION");
            
            u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8 ,128,8,SAVE_cancel_img);
            u8g2_SetDrawColor(&u8g2, 2);
            u8g2_DrawBox(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2)-8, 128, 8);
            u8g2_SetDrawColor(&u8g2, 1);

            break;
        
        case NOT_SELECTED:    
        ESP_LOGI(TAG, "Draw NOT_SELECTED");    
            break;
        
        default:
            break;
        }
        break;
    }

}