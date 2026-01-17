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

menu_textbox_t searchLocMenuTextBox = {
    .textFieldBuffer = searchMenuBuffer,
    .textFieldBufferSize = FIELD_SIZE,    
};

menu_t searchLocMenu = {
    .menuId = SEARCH_LOC_MENU,    
    .startPosY = 8,    
    .textBox = &searchLocMenuTextBox,
    .draw_func = search_loc_menu_draw,
    .event_handler_func = search_loc_menu_handle,
    .enter_func = search_loc_menu_enter,
    .exit_func = search_loc_menu_exit,
    .back_handler_func = go_to_main_menu,   
};

void search_loc_menu_enter()
{
    keypad.textBuffer = searchMenuBuffer;
    // memset(searchLocMenu.textBox->textFieldBuffer, 0, searchLocMenu.textBox->textFieldBufferSize);
    keypad.bufferPos = strlen(keypad.textBuffer) - 1;
    keypad.lastPressedButton = -1;
    keypad.displayCharWidth = 16;
    searchLocMenu.status = EVT_ON_ENTRY;
    searchLocMenu.selectedOption = NOT_SELECTED;
}

menu_t *search_loc_menu_handle(ui_event_e event)
{
    if (searchLocMenu.status == EVT_ON_ENTRY)
        searchLocMenu.status = EVT_NONE;

    switch (event)
    {
    case KEY_ENTER:
        if (searchLocMenu.status != EVT_SEARCH_NOT_FOUND)
        {
            bestLocsNum = find_locations_by_name(searchLocMenu.textBox->textFieldBuffer, bestLocs, LOC_SEARCH_MAX);
            ESP_LOGI("search_loc_menu_handle", "Found locations: %ld", bestLocsNum);
            if (bestLocsNum < 1)
            {
                if (searchLocMenu.status == EVT_SEARCH_NOT_FOUND)
                {
                    esp_timer_stop(display_delay_timer_handle);
                    ui_event_e e = KEY_BACK;
                    xQueueSendToBack(uiEventQueue, &e, pdMS_TO_TICKS(15));
                }
                searchLocMenu.status = EVT_SEARCH_NOT_FOUND;
                display_delay_cb_arg = KEY_BACK;
                esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
            }
            else
            {
                // searchLocMenu.status = EVT_SEARCH_FOUND;
                ui_event_e e = EVT_NEXT_MENU;
                xQueueSendToBack(uiEventQueue, &e, pdMS_TO_TICKS(15));
            }
        }
        break;

    case EVT_NEXT_MENU:
        ESP_LOGI("search_loc_menu_handle", "next menu");
        if (searchLocMenu.nextMenu != NULL)
            return searchLocMenu.nextMenu;
        else
            return &foundLocListMenu;
        break;

    default:
        if (event >= KEY_0 && event <= KEY_SHIFT)
        {
            keypad_button_press(event);
            searchLocMenu.status = EVT_KEYPAD_PRESS;
        }
        break;
    }
    return NULL;
}

void search_loc_menu_exit()
{
    keypad.textBuffer = fieldBuffer;
}

void search_loc_menu_draw()
{
    // const char* TAG = "searchLoc";
    const char *locPromptStr = "Enter Location Name:";
    const char *notFoundStr = "Location not Found";
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);

    uint16_t locPromptPosY = searchLocMenu.startPosY + 2;
    uint16_t textFieldPosY = locPromptPosY + u8g2_GetMaxCharHeight(&u8g2) + 3;
    uint16_t textFieldPosX = 5;

    // ESP_LOGI(TAG,"")
    // u8g2_SetFontPosTop(&u8g2);
    switch (searchLocMenu.status)
    {

    case EVT_ON_ENTRY:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, 0, locPromptPosY, locPromptStr);
        u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, search_img);

        [[fallthrough]];

    case EVT_KEYPAD_PRESS:
        // ESP_LOGI("savetag", "displayCharWidth: %d", displayCharWidth);
        text_field_draw(textFieldPosX, textFieldPosY);
        break;

    case EVT_SEARCH_NOT_FOUND:
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetUTF8Width(&u8g2, notFoundStr) / 2, u8g2_GetDisplayHeight(&u8g2) / 2, notFoundStr);

        break;
    }
}