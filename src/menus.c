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
// #include "bitmaps.h"
#include "littlefs_records.h"
#include <u8g2.h>



static TaskHandle_t scrollHandle;
scroll_data_t sd;
// Wifi records----------------------------------
static wifi_ap_record_t ap_records[BSSID_MAX];
static uint16_t ap_count = 0;
// Found best locations--------------------------
location_t bestLocs[LOC_MATCH_MAX];
uint32_t bestLocsNum;
static char *locNameList[LOC_MATCH_MAX + 1];
// Menu stack------------------------------------
static menu_t* menuStack[MENU_STACK_SIZE];
int stack_top = -1;
void stack_push(menu_t* state) 
{
    if (stack_top < MENU_STACK_SIZE - 1) {
        menuStack[++stack_top] = state;
    }
}
menu_t* stack_pop() 
{
    if (stack_top > -1) {
        return menuStack[stack_top--];
    }
    return NULL; // Stack was empty
}
//Keypad struct and text field buffer------------
char fieldBuffer[FIELD_SIZE];
keypad_t keypad = {
.textBuffer = fieldBuffer,
.bufferSize = sizeof(fieldBuffer),
.bufferPos = -1,
.lastPressedButton = -1,
.displayCharWidth= 16,
};
//Menus functions--------------------------------

static void display_list(menu_t *menu);
void scroll_text_task(void* arg);
static menu_t* go_to_main_menu();

static menu_t *main_menu_handle(int32_t event);
static void main_menu_draw();
static void main_menu_exit();

static void scan_tag_menu_enter();
static menu_t *scan_tag_menu_handle(int32_t event);
static void scan_tag_menu_exit();
static void scan_tag_menu_draw();

static void scan_wifi_menu_enter();
static menu_t *scan_wifi_menu_handle(int32_t event);
static void scan_wifi_menu_exit();
static void scan_wifi_menu_draw();

static void save_tag_menu_enter();
static menu_t *save_tag_menu_handle(int32_t event);
static void save_tag_menu_draw();

static void transmit_menu_enter();
static menu_t *transmit_menu_handle(int32_t event);
static void transmit_menu_exit();
static void transmit_menu_draw();

static void resolve_location_menu_enter();
static menu_t* resolve_location_menu_handle(int32_t event);
static void resolve_location_menu_exit();
static void resolve_location_menu_draw();


//Menus structs----------------------------------------------------------------
char* mainMenuEntries[] = {"Read tag", "Transmit tag", "Dump records", "Search location", "Delete all tags"};
menu_listbox_t mainMenuListBox = {
    .list = mainMenuEntries,
    .listSize =  sizeof(mainMenuEntries) / sizeof(char*),
    .selectedRow = 0,
    .maxRows = 6,
};
menu_listbox_t transmitMenuListBox = {
    .selectedRow = 0,
    .list = locNameList,
    .maxRows = 6,
};
menu_textbox_t saveTagMenuTextBox = {
    .textFieldBuffer = fieldBuffer,
    .textFieldBufferSize = FIELD_SIZE,
    .textFieldPage = 3,
};
//-----------------------------------------------------------------------------
menu_t mainMenu = {
    .menuId = MAIN_MENU,
    .listBox = &mainMenuListBox,
    .startPosY = 8,    
    .draw_func = main_menu_draw,
    .event_handler_func = main_menu_handle,
    .exit_func = main_menu_exit,
};
menu_t scanTagMenu = {
    .menuId = TAG_SCAN,    
    .startPosY = 8,    
    .draw_func = scan_tag_menu_draw,
    .event_handler_func = scan_tag_menu_handle,
    .enter_func = scan_tag_menu_enter,
    .exit_func = scan_tag_menu_exit,
    .back_handler_func = go_to_main_menu,
};
menu_t scanWifiMenu = {
    .menuId = WIFI_SCAN,    
    .startPosY = 8,    
    .draw_func = scan_wifi_menu_draw,
    .enter_func = scan_wifi_menu_enter,
    .event_handler_func = scan_wifi_menu_handle,
    .exit_func = scan_wifi_menu_exit,
    .back_handler_func = go_to_main_menu, 

};

menu_t resolveLocationMenu = {
    .menuId = REWRITE_MATCH_LOC_PROMPT,    
    .startPosY = 8,    

    .draw_func = resolve_location_menu_draw,
    .enter_func = resolve_location_menu_enter,
    .event_handler_func = resolve_location_menu_handle,
    .exit_func = resolve_location_menu_exit,
    .back_handler_func = go_to_main_menu, 

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
menu_t transmitMenu = {
    .menuId = TRANSMIT_TAG_MENU,
    .listBox = &transmitMenuListBox,
    .startPosY = 8,

    .draw_func = transmit_menu_draw,
    .enter_func = transmit_menu_enter,
    .event_handler_func = transmit_menu_handle,
    .exit_func = transmit_menu_exit,    
    .back_handler_func = go_to_main_menu,
    
};

static void list_event_handle(menu_t *menu, int32_t event)
{
    const char* TAG = "list_handle";
    if (menu->listBox == NULL)
    {
        ESP_LOGI(TAG, "Main Menu have no textbox");
        return ;
    }
    if (event == KEY_UP)
    {
        menu->listBox->selectedRow--;
        
        // Wrap-around to bottom if we go above first item
        if (menu->listBox->selectedRow < 0)
            menu->listBox->selectedRow = menu->listBox->listSize - 1;
    }
    else if (event == KEY_DOWN)
    {
        menu->listBox->selectedRow++;
        // Wrap-around to top if we go past last item
        if (menu->listBox->selectedRow > menu->listBox->listSize - 1)
            menu->listBox->selectedRow = 0;
    }
    // Adjust the top row index for scrolling:
    // If selected row moves below the visible item, scroll down
    if (menu->listBox->selectedRow >= menu->listBox->topRowIdx + menu->listBox->maxRows)
        menu->listBox->topRowIdx = menu->listBox->selectedRow - (menu->listBox->maxRows - 1);
    // If selected row moves above the visible item, scroll up
    else if (menu->listBox->selectedRow < menu->listBox->topRowIdx)
        menu->listBox->topRowIdx = menu->listBox->selectedRow;
    // display_list(&mainMenu);
}

//===================================================================
static menu_t* main_menu_handle(int32_t event)
{
    const char* TAG = "main_menu_handle";
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

static void main_menu_draw()
{
    display_list(&mainMenu);
}

static void main_menu_exit()
{
    // ssd1306_clear_screen(devPtr, false);
}

//===================================================================
static void scan_tag_menu_enter()
{
    scanTagMenu.status = EVT_ON_ENTRY;    
     rfid_enable_rx_tag();
}

static menu_t* scan_tag_menu_handle(int32_t event)
{
    if (scanTagMenu.status == EVT_ON_ENTRY) scanTagMenu.status = 0;
    const char* TAG = "scan_tag_handle";
    if (event == EVT_RFID_SCAN_DONE)
    {
        scanTagMenu.status = event; // Send event data to dislapy callback
        // Delay timer to show access points
        display_delay_cb_arg = EVT_NEXT_MENU;
        esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
        if (err != ESP_OK)        
            ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));        
    }
    else if (event == EVT_NEXT_MENU)
    {
        scanWifiMenu.nextMenu = &resolveLocationMenu; // Set next menu in wifi scan menu
        return &scanWifiMenu;
    }
    return NULL;
}

static void scan_tag_menu_exit()
{
    rfid_disable_rx_tx_tag();
    esp_timer_stop(display_delay_timer_handle);
}

static void scan_tag_menu_draw()
{
    const char* tagScanningStr = "Scanning tag...";
    const char* doneStr = "Scan succesfull";

    u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr  );
    u8g2_ClearBuffer(&u8g2);
    // int startPosY = scanTagMenu.startPosY;    
    if (scanTagMenu.status == EVT_RFID_SCAN_DONE)
    {       
        char tagStr[17] = {0};
        sprintf(tagStr, "0x%010llX", currentTag);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2)/2 - u8g2_GetStrWidth(&u8g2, doneStr)/2,
                        u8g2_GetDisplayHeight(&u8g2)/2 - 10, doneStr);
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2)/2 - u8g2_GetStrWidth(&u8g2, tagStr)/2,
                        u8g2_GetDisplayHeight(&u8g2)/2 + 10, tagStr);
        // ssd1306_display_text(devPtr, startPosY, "Scan succesfull", 16, true);
        // ssd1306_display_text(devPtr, startPosY + 2, str, 16, false);
    }
    else if (scanTagMenu.status == EVT_ON_ENTRY)
    {
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2)/2 - u8g2_GetStrWidth(&u8g2, tagScanningStr)/2,
                        u8g2_GetDisplayHeight(&u8g2)/2 - 10, tagScanningStr);
        // ssd1306_display_text(devPtr, scanTagMenu.startPosY, "Scanning tag   ", 16, false);
    }   
    
}

//===================================================================
static void scan_wifi_menu_enter()
{
    const char *TAG = "wifi_enter";
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,       // scan for all SSIDs
        .bssid = NULL,      // scan for all BSSIDs
        .channel = 0,       // 0 = scan all channels
        .show_hidden = true // include hidden SSIDs
    };   
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "esp_wifi_scan_start: %s", esp_err_to_name(err));
    scanWifiMenu.status = EVT_ON_ENTRY;    
}

static menu_t* scan_wifi_menu_handle(int32_t event)
{
    const char *TAG = "wifi_handle";
    if (scanWifiMenu.status == EVT_ON_ENTRY) scanWifiMenu.status = 0;
    switch (event)
    {
    case EVT_WIFI_SCAN_DONE:
        scanWifiMenu.status = event;
        esp_wifi_scan_get_ap_num(&ap_count);
        if (ap_count > BSSID_MAX)
            ap_count = BSSID_MAX;
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);

        if (ap_count == 0) 
        {
            display_delay_cb_arg = KEY_BACK;        
            scanWifiMenu.status = EVT_APS_NOT_FOUND;
        }
        else display_delay_cb_arg = EVT_NEXT_MENU;

         esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
            if (err != ESP_OK)
                ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));
        break;

    case EVT_NEXT_MENU:
        if (scanWifiMenu.nextMenu != NULL)
            return scanWifiMenu.nextMenu;
        else
            return &saveTagMenu;
        break;

    default:
        break;
    }

    return NULL;
}

static void scan_wifi_menu_exit()
{
    esp_timer_stop(display_delay_timer_handle);
}

static void scan_wifi_menu_draw()
{
    // const char* TAG_WIFI_SCAN = "wifi_scan";    
    const char* wifiScanningStr = "Scanning WiFi...";    
    const char* wifiNoAPs = "Wifi APs not found";   
    if (scanWifiMenu.status == EVT_WIFI_SCAN_DONE)
    {
        u8g2_ClearBuffer(&u8g2);
        ssd1306_display_wifi_aps(ap_records,ap_count, scanWifiMenu.startPosY);
        
    } 
    else if (scanWifiMenu.status == EVT_ON_ENTRY || scanWifiMenu.status == EVT_APS_NOT_FOUND) 
    {
        const char* msgStr;
        if (scanWifiMenu.status == EVT_ON_ENTRY) msgStr = wifiScanningStr;
        else if (scanWifiMenu.status == EVT_APS_NOT_FOUND) msgStr = wifiNoAPs;
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr  );    
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2)/2 - u8g2_GetStrWidth(&u8g2, msgStr)/2,
                        u8g2_GetDisplayHeight(&u8g2)/2 - 10, msgStr);
        // ssd1306_clear_screen(devPtr, false);          
        // ssd1306_display_text(devPtr, scanWifiMenu.startPosY, "Scanning WiFi  ", 16, false);
    }    
    
}

static void resolve_location_menu_enter()
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

static menu_t* resolve_location_menu_handle(int32_t event)
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
                sd.exit = true;
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

static void resolve_location_menu_exit()
{
    // ESP_LOGI("resolve_exit", "Stop draw task");
    esp_timer_stop(display_delay_timer_handle);
    sd.exit = true;
    // if (scrollHandle != NULL)
    //     vTaskDelete(scrollHandle);
}

static void resolve_location_menu_draw()
{
    const char* TAG = "resolve_draw";
    const char* matchFoundStr = "Location match found!";
    const char* matchPromptStr1 = "Save new location or";
    const char* matchPromptStr2 = "overwrite old tag?";
    const char* overwriteSuccStr1 = "Overwrite";
    const char* overwriteSuccStr2 = "Successful!!!";

    
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    int charHeight = u8g2_GetMaxCharHeight(&u8g2) - 1;
    // int charWidth = u8g2_GetMaxCharWidth(&u8g2);
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
        int posY = resolveLocationMenu.startPosY + 1;
        u8g2_DrawUTF8(&u8g2, 0, posY, matchFoundStr);
        u8g2_SetDrawColor(&u8g2, 2);
        u8g2_DrawBox(&u8g2, 0, posY, u8g2_GetUTF8Width(&u8g2, matchFoundStr), charHeight);
        u8g2_SetDrawColor(&u8g2, 1);
        // u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
        // charHeight = u8g2_GetMaxCharHeight(&u8g2);
        posY += charHeight + 3;
        u8g2_DrawHLine(&u8g2, 0, posY - 1, 128);
        u8g2_DrawHLine(&u8g2, 0, posY + charHeight + 1, 128);
        int posX = (u8g2_GetDisplayWidth(&u8g2) / 2) - (u8g2_GetStrWidth(&u8g2, (bestLocs[0].name)) / 2);
        if (u8g2_GetUTF8Width(&u8g2, bestLocs[0].name) > u8g2_GetDisplayWidth(&u8g2))
        {
            sd.delayStartStopMs = 1000;
            sd.delayScrollMs = 100;
            sd.font = u8g2.font;
            sd.string = bestLocs[0].name;
            sd.strWidth = u8g2_GetUTF8Width(&u8g2, bestLocs[0].name);
            sd.x = 0;
            sd.y = posY;
            sd.exit = false;

            ESP_LOGI(TAG, "createTask string: %s; strWidth: %d", sd.string, sd.strWidth);
            xTaskCreate(scroll_text_task, "scroll_task", 2048, &sd, 2, &scrollHandle);
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
                u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, SAVE_cancel_img);
                break;

            case OVERWRITE_OPTION:
                ESP_LOGI(TAG, "Draw CANCEL_OPTION");
                u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, SAVE_cancel_img);
                u8g2_SetDrawColor(&u8g2, 2);
                u8g2_DrawBox(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8);
                u8g2_SetDrawColor(&u8g2, 1);

                break;

            case NOT_SELECTED:
            ESP_LOGI(TAG, "Draw NOT_SELECTED");
                u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - 8, 128, 8, save_cancel_unchecked_img);
                break;

            default:
                break;
            }
            break;
        }
}



//===================================================================
static void save_tag_menu_enter()
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

static menu_t* save_tag_menu_handle(int32_t event)
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
                strncpy(loc.name, saveTagMenuTextBox.textFieldBuffer, sizeof(loc.name));

                memcpy(loc.tag, currentTagArray, sizeof(loc.tag));
                append_location(&loc);
            }            
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

static void save_tag_menu_draw()
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

static void transmit_menu_enter()
{
    const char *TAG = "transmit_enter";
    
    transmitMenu.listBox->selectedRow = 0;
    transmitMenu.status = EVT_ON_ENTRY;    

    // for (int i = 0; i < ap_count; i++)
    // {
    //     ESP_LOGI(TAG, "SSID %s", ap_records[i].ssid);
    //     ESP_LOGI(TAG, "BSSID %02x:%02x:%02x:%02x:%02x:%02x", ap_records[i].bssid[0],
    //              ap_records[i].bssid[1],
    //              ap_records[i].bssid[2],
    //              ap_records[i].bssid[3],
    //              ap_records[i].bssid[4],
    //              ap_records[i].bssid[5]);
    //     ESP_LOGI(TAG, "RSSI %d", ap_records[i].rssi);
    // }   
    
    bestLocsNum = find_best_locations_from_scan(ap_records, ap_count, bestLocs, false);
    transmitMenu.listBox->listSize = bestLocsNum + 1;
    
    locNameList[0] = "Auto";
    for (int i = 0; i < bestLocsNum; i++)
    {
        ESP_LOGI(TAG, "Id: %ld\nName: %s\nTag: 0x%010llX", bestLocs[i].id, bestLocs[i].name, rfid_array_to_tag(bestLocs[i].tag));
        locNameList[i + 1] = bestLocs[i].name;
    }   
}

static menu_t* transmit_menu_handle(int32_t event)
{
    if (transmitMenu.status == EVT_ON_ENTRY) transmitMenu.status = 0;
    
    if (bestLocsNum == 0) 
    {
        transmitMenu.status = EVT_NO_MATCH;
        display_delay_cb_arg = KEY_BACK;
        esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
    }
    else
    {   
        list_event_handle(&transmitMenu, event);        
        if (transmitMenu.listBox->selectedRow == 0)        
            vTaskResume(rfidAutoTxHandler);        
        else
        {
            int idx = transmitMenu.listBox->selectedRow - 1;
            if (idx > sizeof(bestLocs)) ESP_LOGE("transmit_menu_handle", "idx > sizeof(bestLocs)");
            uint64_t rawTag = rfid_arr_tag_to_raw_tag(bestLocs[idx].tag);
            rfid_enable_tx_raw_tag(rawTag);
        }
    }    
    return NULL;    
}

static void transmit_menu_exit()
{
    vTaskSuspend(rfidAutoTxHandler);
    rfid_disable_rx_tx_tag();
    esp_timer_stop(display_delay_timer_handle);
}

static void transmit_menu_draw()
{
    const char* noMatchStr = "Tag not found";
    
    if (transmitMenu.status == EVT_NO_MATCH)
    {        
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_6x13B_tr  );
        u8g2_DrawUTF8(&u8g2, u8g2_GetDisplayWidth(&u8g2) / 2 - u8g2_GetStrWidth(&u8g2, noMatchStr) / 2,
                      u8g2_GetDisplayHeight(&u8g2) / 2 - 10, noMatchStr);
    }
    else        
        display_list(&transmitMenu);
    
}


static menu_t* go_to_main_menu()
{
    menu_t *returnMenu = NULL;
    while (1)
    {
        returnMenu = stack_pop();
        if (returnMenu == NULL)
            return NULL;
        if (returnMenu->menuId == MAIN_MENU)
            return returnMenu;
    }
}

//===================================================================
void ssd1306_display_wifi_aps(wifi_ap_record_t *ap_records, uint16_t ap_count, uint32_t startPosY)
{
    const int maxPages = (u8g2_GetDisplayHeight(&u8g2) - startPosY) / (u8g2_GetMaxCharHeight(&u8g2)+2);
    u8g2_SetFont(&u8g2, u8g2_font_NokiaSmallBold_tr);
    char str[30];
    // ssd1306_clear_screen(devPtr, false);
    for (uint8_t i = 0; i < 5 && i < ap_count && i < maxPages - startPosY; i++)
    {
        sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x %d",
                ap_records[i].bssid[0],
                ap_records[i].bssid[1],
                ap_records[i].bssid[2],
                ap_records[i].bssid[3],
                ap_records[i].bssid[4],
                ap_records[i].bssid[5],
                ap_records[i].rssi);
        u8g2_DrawUTF8(&u8g2, 1, (i) * (u8g2_GetMaxCharHeight(&u8g2) + 2), str);
        // ssd1306_display_text(devPtr, i + startPosY, str, strlen(str), false);
    }
}

static void display_list(menu_t* menu) 
{        
    uint8_t listSize = menu->listBox->listSize;    
    u8g2_SetFont(&u8g2, u8g2_font_7x13_tr);
    int maxDisplayItems = (u8g2_GetDisplayHeight(&u8g2) - menu->startPosY) / (u8g2_GetMaxCharHeight(&u8g2)+1);
    if (menu->listBox->maxRows > maxDisplayItems) menu->listBox->maxRows = maxDisplayItems;
    
    ESP_LOGI("display list", "maxRows: %d", menu->listBox->maxRows);
    u8g2_ClearBuffer(&u8g2);
    for (int i = 0; i < (menu->listBox->maxRows) && (i < listSize); i++)
    {
        int idx = i + menu->listBox->topRowIdx;
        // Ensure you don't read past the end of the list if there's a logic error elsewhere
        if (idx >= listSize) continue;       
        u8g2_DrawButtonUTF8(&u8g2, 5, (i) * (u8g2_GetMaxCharHeight(&u8g2) + 1) + menu->startPosY,
                            idx == menu->listBox->selectedRow ? U8G2_BTN_INV : U8G2_BTN_BW0,
                            u8g2_GetDisplayWidth(&u8g2) - 5 * 2, 5, 0, menu->listBox->list[idx]);        
    }
    
}


#define NUM_BUTTONS 10
#define MAX_CHARS_PER_KEY 5
const char keyMap[NUM_BUTTONS][MAX_CHARS_PER_KEY] = {
    {' ', '0'},               // Key '0'
    {'1', '.', ',', '!', '?'},// Key '1'
    {'a', 'b', 'c', '2'},     // Key '2'
    {'d', 'e', 'f', '3'},     // Key '3'
    {'g', 'h', 'i', '4'},     // Key '4'
    {'j', 'k', 'l', '5'},     // Key '5'
    {'m', 'n', 'o', '6'},     // Key '6'
    {'p', 'q', 'r', 's', '7'},// Key '7'
    {'t', 'u', 'v', '8'},     // Key '8'
    {'w', 'x', 'y', 'z', '9'} // Key '9'
};

const uint8_t keyMapLen[NUM_BUTTONS] = {
    2, // For Key '0'
    5, // For Key '1'
    4, // For Key '2'
    4, // For Key '3'
    4, // For Key '4'
    4, // For Key '5'
    4, // For Key '6'
    5, // For Key '7'
    5, // For Key '8'
    5  // For Key '9'
};

void keypad_button_press(int8_t pressedButton)
{
    const char* TAG = "keypad_button_press";
    if (keypad.textBuffer == NULL || keypad.bufferSize < 1)
    {
        ESP_LOGE(TAG,"textBuffer is NULL or bufferSize < 1");
        return;
    }

    // Stop any pending confirmation timer, as a new action is happening
    esp_timer_stop(confirmation_timer_handle);
    int32_t currentTick = esp_cpu_get_cycle_count();

    switch (pressedButton)
    {
    case KEY_CLEAR_CHAR:
        if (keypad.lastPressedButton != -1)
        {
            esp_timer_stop(confirmation_timer_handle); // cancel auto‐commit
            keypad.lastPressedButton = -1;
            keypad.currentPressCount = 0;
            // remove the “pending” slot
            int pendingIndex = keypad.bufferPos + 1;
            keypad.textBuffer[pendingIndex] = '\0';
        }
        // Otherwise, backspace the last confirmed character (if any).
        else if (keypad.bufferPos >= 0)
        {
            // remove the char at bufferPos
            keypad.textBuffer[keypad.bufferPos] = '\0';
            keypad.bufferPos--;
        }
        break;
        
    case KEY_SHIFT:
        keypad.letterIsCapital = !keypad.letterIsCapital;
        break;

    default:
        if ((pressedButton >= KEY_0) && (pressedButton <= KEY_9))
        {
            // Check if this is a continuation of the current multi-tap sequence.
            if (keypad.lastPressedButton == pressedButton &&
                (currentTick - keypad.lastTick) < (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL))
            {
                // Cycle among the possible characters.
                keypad.currentPressCount = (keypad.currentPressCount + 1) % keyMapLen[pressedButton];
            }
            else
            {
                // A new key or a timeout: commit any pending character if one exists.
                if (keypad.lastPressedButton != -1)
                {
                    if (keypad.bufferPos < keypad.bufferSize - 2)                    
                        keypad.bufferPos++; // Commit the pending character.                    
                    else                    
                        ESP_LOGW(TAG, "Buffer full, cannot commit pending character");                    
                }
                // Begin a new multi-tap sequence.
                keypad.letterIsCapital = 0;
                keypad.lastPressedButton = pressedButton;
                keypad.currentPressCount = 0;
            }
            // Write/update the pending character at index (bufferPos+1) in the same buffer.
            int pendingIndex = keypad.bufferPos + 1;
            if (pendingIndex < keypad.bufferSize - 1)
            { // Make sure there is room for the char and a null terminator.
                char newChar = keyMap[pressedButton][keypad.currentPressCount];
                if ((newChar >= 'a') && (newChar <= 'z') && keypad.letterIsCapital)
                    newChar -= 0x20; // Capitalize if needed.
                keypad.textBuffer[pendingIndex] = newChar;
                keypad.textBuffer[pendingIndex + 1] = '\0';
            }
            else            
                ESP_LOGW(TAG, "Buffer full, cannot add new pending character");            

            // Start the confirmation timer to commit the pending character after a delay.
            esp_err_t err = esp_timer_start_once(confirmation_timer_handle, 1000000ULL);
            if (err != ESP_OK)
                ESP_LOGE(TAG, "Failed to start confirmation timer: %s", esp_err_to_name(err));
        }      

        keypad.lastTick = currentTick;
        int32_t event = EVT_KEYPAD_PRESS;
        xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));

        break;
    }
}

// Timer callback to confirm the pending character if no other key is pressed
void confirmation_timer_callback(void *arg)
{
    const char *TAG = "confirmation_timer_callback";
    if (keypad.textBuffer == NULL || keypad.bufferSize == 0)
        return;

    // If a pending character exists (indicated by an active sequence), commit it.
    if (keypad.lastPressedButton != -1)
    {
        if (keypad.bufferPos < keypad.bufferSize - 2)        
            keypad.bufferPos++; // The pending character now becomes confirmed.        
        else        
            ESP_LOGW(TAG, "Buffer full on commit");
        
        // Reset multi-tap state.
        keypad.lastPressedButton = -1;
        keypad.currentPressCount = 0;
    }

    // int32_t event = EVT_KEYPAD_PRESS;
    // xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
}

menu_t* currentMenu = &mainMenu;

void ui_handler_task(void* args)
{
    int32_t event;
    if (currentMenu->draw_func)
    {
        currentMenu->draw_func();
        u8g2_SendBuffer(&u8g2);
    }
    for (;;)
    {        
        if (xQueueReceive(uiEventQueue, &event, pdMS_TO_TICKS(30000)) == pdTRUE)
        {            
            ESP_LOGI("ui_handler","Event: %ld\tMenu: %d", event, currentMenu->menuId);
            // Allow back navigation from any state
            if (event == KEY_BACK)
            {
                if (currentMenu->back_handler_func != NULL)
                {
                     menu_t *perviousMenu = currentMenu->back_handler_func();
                    if (perviousMenu != NULL)                    
                    {                       
                        if (currentMenu->exit_func)
                            currentMenu->exit_func();                        
                        currentMenu = perviousMenu;
                        if (currentMenu->enter_func)
                            currentMenu->enter_func();                            
                    }
                }
                else
                {
                    menu_t *perviousMenu = stack_pop();
                    if (perviousMenu != NULL)
                    {
                        if (currentMenu->exit_func)
                            currentMenu->exit_func();

                        currentMenu = perviousMenu;

                        if (currentMenu->enter_func)
                            currentMenu->enter_func();
                    }
                }
            }
            else
            {   // Let the current state handle the event
                if (currentMenu->event_handler_func)
                {
                    menu_t *nextMenu = currentMenu->event_handler_func(event);

                    // Check if a state transition is needed
                    if (nextMenu != NULL)
                    {
                        // Push current state to stack for "back" functionality
                        stack_push(currentMenu);

                        // Perform exit action of old state
                        if (currentMenu->exit_func)
                            currentMenu->exit_func();

                        // Officially change state
                        currentMenu = nextMenu;

                        // Perform entry action of new state
                        if (currentMenu->enter_func)
                            currentMenu->enter_func();
                    }
                }
            }

            // Redraw the screen with the current (or new) state
            if (currentMenu->draw_func) 
            {
                currentMenu->draw_func();
                if (xSemaphoreTake(drawMutex, portMAX_DELAY) == pdTRUE)
                {
                    u8g2_SendBuffer(&u8g2);
                    xSemaphoreGive(drawMutex);
                }
            }

            
        }
    }    
}

void display_delay_timer_callback(void* event)
{    
    xQueueSendToBack(uiEventQueue, (ui_event_e*)event, pdMS_TO_TICKS(15));
}

void tag_tx_cycle_callback()
{
    int i = 0;
    if (bestLocsNum <= 0) return;

    for (;;)
    {
        uint64_t rawTag = rfid_arr_tag_to_raw_tag(bestLocs[i].tag);
        rfid_enable_tx_raw_tag(rawTag);
        // ESP_LOGI("tag_tx_cycle", "loc: %s\ttag: 0x%010llX", bestLocs[i].name, tag);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        i++;
        if (i >= bestLocsNum)
            i = 0;
    }
}

void scroll_text_task(void* arg)
{
    scroll_data_t *scrollPtr = arg;
    int offset = 0;
    const char* TAG = "scroll";
    // const uint8_t *prevFont = u8g2.font;
    // if (scrollPtr != NULL)
        // u8g2_SetFont(&u8g2, scrollPtr->font);   
    // int strLength = u8x8_GetUTF8Len(&u8g2.u8x8, scrollPtr->string);
    int dispayWidth = u8g2_GetDisplayWidth(&u8g2);
    int charHeight = u8g2_GetMaxCharHeight(&u8g2);
    ESP_LOGI(TAG, "strWidth=%d dWidth=%d", scrollPtr->strWidth, dispayWidth);
    vTaskDelay(pdMS_TO_TICKS(50));

    for (;;)
    {
        // if (scrollPtr->font != NULL)
        //     u8g2_SetFont(&u8g2, scrollPtr->font);
        if (scrollPtr->exit)
            vTaskDelete(NULL);

        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawBox(&u8g2, scrollPtr->x, scrollPtr->y, dispayWidth, charHeight);
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawUTF8(&u8g2, scrollPtr->x - offset, scrollPtr->y, scrollPtr->string);

        // u8g2_SetFont(&u8g2, prevFont);
        if (xSemaphoreTake(drawMutex, portMAX_DELAY) == pdTRUE)
        {
            u8g2_UpdateDisplayArea(&u8g2, scrollPtr->x / 8, scrollPtr->y / 8, dispayWidth / 8, charHeight / 8 + 2);
            xSemaphoreGive(drawMutex);
        }    

        if (offset == 0)
        {
            // ESP_LOGI(TAG, "offset == 0 delay %d", scrollPtr->delayStartStopMs); 
            vTaskDelay(pdMS_TO_TICKS(scrollPtr->delayStartStopMs));
        }
           
        if (scrollPtr->strWidth - offset < dispayWidth)
        {
            offset = 0;
            // ESP_LOGI(TAG, "sWidth - o < dWidth delay %d", scrollPtr->delayStartStopMs);
            vTaskDelay(pdMS_TO_TICKS(scrollPtr->delayStartStopMs));            
        }
        else
        {
            offset += 2;
            // ESP_LOGI(TAG, "offset += 2 delay %d",scrollPtr->delayScrollMs);
            vTaskDelay(pdMS_TO_TICKS(scrollPtr->delayScrollMs));
        }
    }
}