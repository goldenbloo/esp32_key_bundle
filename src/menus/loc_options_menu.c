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

char *options[] = {"Transmit tag", "Change location name", "Update APs", "Update tag", "Delete location"};
menu_listbox_t locOptionsMenuListBox = {        
    .selectedRow = 0,
    .list = options,
    .listSize = sizeof(options) / sizeof(char *),
    .maxRows = 6,
};

menu_t locOptionsMenu  = {
    .menuId = LOC_EDIT_OPTIONS_MENU,    
    .listBox = &locOptionsMenuListBox,
    .startPosY = 8,  
    .draw_func = loc_options_menu_draw,
    .event_handler_func = loc_options_menu_handle,
    .enter_func = loc_options_menu_enter,   
};

void loc_options_menu_enter()
{
    // const char *TAG = "loc_options_menu_enter";
    locOptionsMenu.listBox->selectedRow = 0;
    locOptionsMenu.status = EVT_ON_ENTRY;    
}

menu_t *loc_options_menu_handle(int32_t event)
{
    if (locOptionsMenu.status == EVT_ON_ENTRY)
        locOptionsMenu.status = 0;

    if (event == KEY_ENTER)
    {
        // TODO 
        switch (locOptionsMenu.listBox->selectedRow)
        {
        case 0:
            // Transmit tag
            break;

        case 1:
            // Change location name
            break;

        case 2:
            // Update APs
            break;

        case 3:
            // Update tag
            break;

        case 4:
            // Delete location
            break;

        default:
            break;
        }
    }
    else
        list_event_handle(&locOptionsMenu, event);

    return NULL;
}

void loc_options_menu_draw()
{
    display_list(&locOptionsMenu);
}

