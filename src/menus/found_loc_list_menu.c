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

menu_listbox_t foundLocListMenuListBox = {        
    .selectedRow = 0,
    .list = locNameList,
    .maxRows = 6,
};

menu_t foundLocListMenu = {
    .menuId = FOUND_LOC_LIST_MENU,    
    .listBox = &foundLocListMenuListBox,
    .startPosY = 8,  
    .draw_func = found_loc_list_menu_draw,
    .event_handler_func = found_loc_list_menu_handle,
    .enter_func = found_loc_list_menu_enter,
    .back_handler_func = go_to_main_menu,   
};

void found_loc_list_menu_enter()
{
    const char *TAG = "found_loc_list_menu_enter";
    foundLocListMenu.listBox->selectedRow = 0;
    foundLocListMenu.status = EVT_ON_ENTRY;
    foundLocListMenu.listBox->listSize = bestLocsNum;

    for (uint8_t i = 0; i < bestLocsNum; i++)
    {
        ESP_LOGI(TAG, "Id: %ld\nName: %s\nTag: 0x%010llX", bestLocs[i].id, bestLocs[i].name, rfid_array_to_tag(bestLocs[i].tag));
        locNameList[i] = bestLocs[i].name;
    }   

}

menu_t *found_loc_list_menu_handle(int32_t event)
{
     if (foundLocListMenu.status == EVT_ON_ENTRY)
         foundLocListMenu.status = 0;

     
     if (event == KEY_ENTER)
     {
         chosenLoc = bestLocs[foundLocListMenu.listBox->selectedRow];
         return &locOptionsMenu;
     }
     else
        list_event_handle(&foundLocListMenu, event);
     
     return NULL;
}

void found_loc_list_menu_draw()
{
    display_list(&foundLocListMenu);
}