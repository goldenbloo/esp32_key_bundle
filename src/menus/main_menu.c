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


char *mainMenuEntries[] = {"Read tag", "Transmit tag", "Dump records", "Search location", "Delete all tags"};
menu_listbox_t mainMenuListBox = {
    .list = mainMenuEntries,
    .listSize = sizeof(mainMenuEntries) / sizeof(char *),
    .selectedRow = 0,
    .maxRows = 6,
};

menu_t mainMenu = {
    .menuId = MAIN_MENU,
    .listBox = &mainMenuListBox,
    .startPosY = 8,
    .draw_func = main_menu_draw,
    .event_handler_func = main_menu_handle,
    .exit_func = main_menu_exit,
};

menu_t *main_menu_handle(int32_t event)
{
    const char *TAG = "main_menu_handle";
    list_event_handle(&mainMenu, event);

    if (event == KEY_ENTER)
    {
        switch (mainMenu.listBox->selectedRow)
        {
        case 0: // Receive tag choice
            ESP_LOGI(TAG, "enter to receive");
            return &scanTagMenu;
            break;

        case 1: // Transmit choice
            ESP_LOGI(TAG, "enter to trasmit");
            scanWifiMenu.nextMenu = &transmitMenu;
            return &scanWifiMenu;
            break;

        case 2: // Dump locations choice
            read_all_locations();
            break;

        case 3: // Search location

            break;

        case 4: // Delete all locations choice
            clear_all_locations();
            break;

        default:
            break;
        }
    }
    return NULL;
}

void main_menu_draw()
{
    display_list(&mainMenu);
}

void main_menu_exit()
{
    // ssd1306_clear_screen(devPtr, false);
}