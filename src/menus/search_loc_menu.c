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

menu_textbox_t searchLocMenuTextBox = {
    .textFieldBuffer = fieldBuffer,
    .textFieldBufferSize = FIELD_SIZE,    
};

menu_t searchLocMenu = {
    .menuId = SAVE_TAG_MENU,    
    .startPosY = 8,    
    .textBox = &searchLocMenuTextBox,
    .draw_func = search_loc_menu_draw,
    .event_handler_func = search_loc_menu_handle,
    .enter_func = search_loc_menu_enter,
    .back_handler_func = go_to_main_menu,   
};

void search_loc_menu_enter()
{
     if (searchLocMenu.textBox == NULL){
        ESP_LOGE("wifi_enter", "Textbox in searchLocMenu in NULL"); 
        return;
    }
    memset(searchLocMenu.textBox->textFieldBuffer, 0, searchLocMenu.textBox->textFieldBufferSize); 
    keypad.bufferPos = -1;   
    keypad.lastPressedButton = -1;
    keypad.displayCharWidth = 16;
    searchLocMenu.status = EVT_ON_ENTRY;
    searchLocMenu.selectedOption = NO_OPTION;
}

menu_t *search_loc_menu_handle(int32_t event)
{
    if (searchLocMenu.status == EVT_ON_ENTRY) searchLocMenu.status = 0;
    switch (event)
    {
     case EVT_KEYPAD_PRESS:
        searchLocMenu.status = EVT_KEYPAD_PRESS;
        break;
    
    default:
        break;
    }
    return NULL;
}

void search_loc_menu_draw()
{
    const char* TAG = "searchLoc";
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
        u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, search_img);

        [[fallthrough]];

    case EVT_KEYPAD_PRESS:
        // ESP_LOGI("savetag", "displayCharWidth: %d", displayCharWidth);
        u8g2_SetDrawColor(&u8g2, 0); // 0 = background
        u8g2_DrawBox(&u8g2, 0, textFieldPosY, u8g2_GetDisplayWidth(&u8g2), u8g2_GetMaxCharHeight(&u8g2));
        u8g2_SetDrawColor(&u8g2, 1); // Reset to foreground for next text
        u8g2_DrawFrame(&u8g2, textFieldPosX - 2, textFieldPosY, (u8g2_GetMaxCharWidth(&u8g2) + 1) * displayCharWidth + 8, u8g2_GetMaxCharHeight(&u8g2) + 2);
        u8g2_DrawUTF8(&u8g2, textFieldPosX, textFieldPosY, startPtr);
        break;

   
        break;
    }
}