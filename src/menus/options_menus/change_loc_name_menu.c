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

menu_textbox_t changeLocNameMenuTextBox = {
    .textFieldBuffer = fieldBuffer,
    .textFieldBufferSize = FIELD_SIZE,    
};

menu_t changeLocNameMenu  = {
    .menuId = LOC_NAME_EDIT_MENU,    
    .textBox = &changeLocNameMenuTextBox,
    .startPosY = 8,  
    .draw_func = change_loc_name_menu_draw,
    .event_handler_func = change_loc_name_menu_handle,
    .enter_func = change_loc_name_menu_enter,   
};

void change_loc_name_menu_enter()
{
    memset(changeLocNameMenu.textBox->textFieldBuffer, 0, changeLocNameMenu.textBox->textFieldBufferSize);
    strncpy(changeLocNameMenu.textBox->textFieldBuffer, currentLoc.name, changeLocNameMenu.textBox->textFieldBufferSize);
    keypad.bufferPos = strlen(currentLoc.name) - 1;
    keypad.lastPressedButton = -1;
    keypad.displayCharWidth = 16;
    changeLocNameMenu.status = EVT_ON_ENTRY;  
}

menu_t *change_loc_name_menu_handle(ui_event_e event)
{
    if (changeLocNameMenu.status == EVT_ON_ENTRY)
        changeLocNameMenu.status = EVT_NONE;

    if (updateWifiMenu.status != EVT_SAVE_SUCCESS && updateWifiMenu.status != EVT_SAVE_FAIL)
    {
        switch (event)
        {
        case KEY_ENTER:
            // TODO Save name
            memset(currentLoc.name, 0, FIELD_SIZE);
            strncpy(currentLoc.name, changeLocNameMenu.textBox->textFieldBuffer, FIELD_SIZE - 1);
            if (write_location(&currentLoc))
                changeLocNameMenu.status = EVT_SAVE_SUCCESS;
            else
                changeLocNameMenu.status = EVT_SAVE_FAIL;
            display_delay_cb_arg = KEY_BACK;
            esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
            break;

        default:
            if (event >= KEY_0 && event <= KEY_SHIFT)
            {
                keypad_button_press(event);
                changeLocNameMenu.status = EVT_KEYPAD_PRESS;
            }
            break;
        }
    }

    return NULL;
}

void change_loc_name_menu_draw()
{
    const char *newNamePromptStr = "Enter New Loc Name:";
    const char *locNameSavedStr = "Location Name Saved";
    const char *locSaveFailedStr = "ERROR: Save Failed";
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

    uint16_t locPromptPosY = changeLocNameMenu.startPosY + 2;
    uint16_t textFieldPosY = locPromptPosY + u8g2_GetMaxCharHeight(&u8g2) + 3;
    uint16_t textFieldPosX = 5;

    // ESP_LOGI(TAG,"")
    // u8g2_SetFontPosTop(&u8g2);
    switch (changeLocNameMenu.status)
    {
    case EVT_SAVE_SUCCESS:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locNameSavedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locNameSavedStr);
        break;

    case EVT_SAVE_FAIL:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, locSaveFailedStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, locSaveFailedStr);
        break;

    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, 0, locPromptPosY, newNamePromptStr);
        u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, save_img);
        [[fallthrough]];

    case EVT_KEYPAD_PRESS:
        // ESP_LOGI("new_loc_name", "bufferPos: %d\ttextBuffer: %s",keypad.bufferPos, keypad.textBuffer);
        text_field_draw(textFieldPosX, textFieldPosY);
        break;

    
    }
}


