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
// Menu stack------------------------------------
menu_t* menu_stack[MENU_STACK_SIZE];
int stack_top = -1;
//Keypad struct and text field buffer------------
char fieldBuffer[FIELD_SIZE];
keypad_t keypad = {
.textBuffer = fieldBuffer,
.bufferSize = sizeof(fieldBuffer),
.lastPressedButton = -1,
.displayCharWidth= 16,
};
//Menus functions--------------------------------
static void display_list(menu_t *menu);

static menu_t *main_menu_handle(char event);
static void main_menu_draw();
static void main_menu_exit();

static void scan_tag_menu_enter();
static menu_t *scan_tag_menu_handle(char event);
static void scan_tag_menu_exit();
static void scan_tag_menu_draw();

static void scan_wifi_menu_enter();
static menu_t *scan_wifi_menu_handle(char event);
static void scan_wifi_menu_exit();
static void scan_wifi_menu_draw();

static void save_tag_menu_enter();
static menu_t *save_tag_menu_handle(char event);
static void save_tag_menu_draw();

//Menus structs----------------------------------
char* mainMenuList[] = {"Transmit tag", "Read tag", "test", "test111", "t", "test", "test3333333", "abcdefg"};
menu_t mainMenu = {
    .menuWindow = MAIN_MENU,
    .list = mainMenuList,
    .listSize = 8,
    .selectedRow = 0,
    .startPage = 4,
    .maxPages = 7,
    .draw_func = main_menu_draw,
    .event_handler_func = main_menu_handle,
    .exit_func = main_menu_exit,
};
menu_t scanTagMenu = {
    .menuWindow = TAG_SCAN,    
    .startPage = 1,
    .maxPages = 7,
    .draw_func = scan_tag_menu_draw,
    .event_handler_func = scan_tag_menu_handle,
    .enter_func = scan_tag_menu_enter,
    .exit_func = scan_tag_menu_exit,
};
menu_t scanWifiMenu = {
    .menuWindow = WIFI_SCAN,    
    .startPage = 1,
    .maxPages = 7,
    .draw_func = scan_wifi_menu_draw,
    .enter_func = scan_wifi_menu_enter,
    .event_handler_func = scan_wifi_menu_handle, 

};
menu_t saveTagMenu = {
    .menuWindow = SAVE_TAG_PROMPT,    
    .startPage = 1,
    .maxPages = 7,
    .textFieldBuffer = fieldBuffer,
    .textFieldBufferSize = FIELD_SIZE,
    .textFieldPage = 3,
    .draw_func = save_tag_menu_draw,
    .event_handler_func = save_tag_menu_handle,
    .enter_func = save_tag_menu_enter,
   
};

//===================================================================
static menu_t* main_menu_handle(char event)
{
    const char* TAG = "main_menu_handle";
    if (event == KEY_UP)
    {
        mainMenu.selectedRow--;
        // Wrap-around to bottom if we go above first item
        if (mainMenu.selectedRow < 0)
            mainMenu.selectedRow = mainMenu.listSize - 1;
    }
    else if (event == KEY_DOWN)
    {
        mainMenu.selectedRow++;
        // Wrap-around to top if we go past last item
        if (mainMenu.selectedRow > mainMenu.listSize - 1)
            mainMenu.selectedRow = 0;
    }
    // Adjust the top row index for scrolling:
    // If selected row moves below the visible page, scroll down
    if (mainMenu.selectedRow >= mainMenu.topRowIdx + mainMenu.maxPages)
        mainMenu.topRowIdx = mainMenu.selectedRow - (mainMenu.maxPages - 1);
    // If selected row moves above the visible page, scroll up
    else if (mainMenu.selectedRow < mainMenu.topRowIdx)
        mainMenu.topRowIdx = mainMenu.selectedRow;
    // display_list(&mainMenu);

    if (event == KEY_ENTER)
    {       
        switch (mainMenu.selectedRow)
        {
        case 0: // Receive tag choice
            ESP_LOGI(TAG, "enter to receive");
            return &scanTagMenu;
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
    ssd1306_clear_screen(devPtr, false);
}

//===================================================================
static void scan_tag_menu_enter()
{
    scanTagMenu.status = EVT_ON_ENTRY;    
     enable_rx_tag();
}

static menu_t* scan_tag_menu_handle(char event)
{
    if (scanTagMenu.status == EVT_ON_ENTRY) scanTagMenu.status = 0;
    const char* TAG = "scan_tag_handle";
    if (event == EVT_RFID_SCAN_DONE)
    {
        scanTagMenu.status = event; // Send event data to dislapy callback
        // Delay timer to show access points
        esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
        if (err != ESP_OK)        
            ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));
        
    }
    else if (event == EVT_NEXT_MENU)
    {
        return &scanWifiMenu;
    }
    return NULL;
}

static void scan_tag_menu_exit()
{
    disable_rx_tx_tag();
    esp_timer_stop(display_delay_timer_handle);
}

static void scan_tag_menu_draw()
{
    ssd1306_clear_screen(devPtr, false);
    int startPage = scanTagMenu.startPage;    
    if (scanTagMenu.status == EVT_RFID_SCAN_DONE)
    {       
        char str[17];
        sprintf(str, "0x%010" PRIX64, currentTag);
        ssd1306_display_text(devPtr, startPage, "Scan succesfull", 16, true);
        ssd1306_display_text(devPtr, startPage + 2, str, 16, false);
    }
    else if (scanTagMenu.status == EVT_ON_ENTRY)
    {
        ssd1306_display_text(devPtr, scanTagMenu.startPage, "Scanning tag   ", 16, false);
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

static menu_t* scan_wifi_menu_handle(char event)
{
    const char* TAG = "wifi_handle";
    if (scanWifiMenu.status == EVT_ON_ENTRY) scanWifiMenu.status = 0;
    if (event == EVT_WIFI_SCAN_DONE)
    {
        scanWifiMenu.status = event;
        esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
        if (err != ESP_OK)        
            ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err)); 
    }    
    else if (event == EVT_NEXT_MENU)
    {
        return &saveTagMenu;
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
    
    if (scanWifiMenu.status == EVT_WIFI_SCAN_DONE)
    {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        // ESP_LOGI(TAG_WIFI_SCAN, "Total APs scanned = %d", ap_count);
        // for (int i = 0; i < ap_count; i++)
        // {
        //     ESP_LOGI(TAG_WIFI_SCAN, "SSID \t\t%s", ap_records[i].ssid);
        //     ESP_LOGI(TAG_WIFI_SCAN, "BSSID \t\t%02x:%02x:%02x:%02x:%02x:%02x", ap_records[i].bssid[0],
        //              ap_records[i].bssid[1],ap_records[i].bssid[2],ap_records[i].bssid[3],
        //              ap_records[i].bssid[4],ap_records[i].bssid[5]);
        //     ESP_LOGI(TAG_WIFI_SCAN, "RSSI \t\t%d", ap_records[i].rssi);
        //     ESP_LOGI(TAG_WIFI_SCAN, "Authmode \t%d", ap_records[i].authmode);
        // }
        ssd1306_clear_screen(devPtr, false);
        ssd1306_display_wifi_aps(ap_records,ap_count, scanWifiMenu.startPage);
        free(ap_records);
    } 
    else if (scanWifiMenu.status == EVT_ON_ENTRY) 
    {
        ssd1306_clear_screen(devPtr, false);          
        ssd1306_display_text(devPtr, scanWifiMenu.startPage, "Scanning WiFi  ", 16, false);
    }
}

//===================================================================
static void save_tag_menu_enter()
{        
    memset(saveTagMenu.textFieldBuffer, 0, saveTagMenu.textFieldBufferSize);    
    keypad.lastPressedButton = -1;
    keypad.displayCharWidth = 16;
    saveTagMenu.status = EVT_ON_ENTRY;
}

static menu_t* save_tag_menu_handle(char event)
{
    if (saveTagMenu.status == EVT_ON_ENTRY) saveTagMenu.status = 0;
    if (event >= KEY_0 && event <= KEY_SHIFT) 
    {
        keypad_button_press(event);
        saveTagMenu.status = EVT_KEYPAD_PRESS;
    }   
    if (event == EVT_KEYPAD_PRESS)
        saveTagMenu.status = EVT_KEYPAD_PRESS;
           
    return NULL;
}

static void save_tag_menu_draw()
{
    // ESP_LOGI("save_tag_draw","Status: %d", saveTagMenu.status);
    if (saveTagMenu.status == EVT_KEYPAD_PRESS)
        ssd1306_display_text(devPtr,
                             saveTagMenu.textFieldPage,
                             keypad.bufferPos < keypad.displayCharWidth ? keypad.textBuffer : keypad.textBuffer + (keypad.bufferPos - keypad.displayCharWidth + 1),
                             keypad.displayCharWidth, false);

    else if (saveTagMenu.status == EVT_ON_ENTRY)
    {
        ssd1306_clear_screen(devPtr, false);
        ssd1306_display_text(devPtr, saveTagMenu.textFieldPage - 1, "Location name: ", 16, false);
    }
}



//===================================================================
void ssd1306_display_wifi_aps(wifi_ap_record_t *ap_records, uint16_t ap_count, uint32_t startPage)
{
    const int maxPages = devPtr->_pages;
    char str[20];
    // ssd1306_clear_screen(devPtr, false);
    for (uint8_t i = 0; i < 5 && i < ap_count && i < maxPages - startPage; i++)
    {
        sprintf(str, "%02x:%02x:%02x:%02x %d",
                ap_records[i].bssid[2],
                ap_records[i].bssid[3],
                ap_records[i].bssid[4],
                ap_records[i].bssid[5],
                ap_records[i].rssi);
        ssd1306_display_text(devPtr, i + startPage, str, strlen(str), false);
    }
}

static void display_list(menu_t* menu) 
{
    const int maxPages = devPtr->_pages;
    char buffer[17];
    uint8_t listSize = menu->listSize;

    // The rest of your function remains exactly the same...
    if (menu->startPage + menu->maxPages > maxPages)
    {    
        menu->maxPages = maxPages - menu->startPage;
        ESP_LOGE("menu_list", "startPage + maxPages > maxPages; max pages = %d", menu->maxPages);
    }
    
    for (int page = 0; page < (menu->maxPages) && (page < listSize); page++)
    {
        int idx = page + menu->topRowIdx;
        // Ensure you don't read past the end of the list if there's a logic error elsewhere
        if (idx >= listSize) continue;        
        
        strncpy(buffer, menu->list[idx], sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination

        // No need for separate memset, strncpy + manual null termination is safer
        ssd1306_display_text(devPtr, page + menu->startPage, buffer, 16, idx == menu->selectedRow ? 1 : 0);
    }
}

// Display location save prompt
void display_loc_save(QueueHandle_t keyEventQueue)
{
    #define OPTION_YES      1
    #define OPTION_NO       0
    #define OPTION_CHANGE   2
    char action;
    char row0[] = "Save location";
    char row1[] = "with name:";
    char locName[] = "location_1";
    const uint8_t yes_no[] = {
    0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x31, 0x80, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x39, 0x80, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x0c, 0xc7, 0x87, 0xc0, 0x00, 0x01, 0x80, 0x00, 0x00, 0x3d, 0x9e, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x07, 0x8c, 0xcc, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x37, 0xb3, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x03, 0x0f, 0xc7, 0x80, 0x00, 0x01, 0x80, 0x00, 0x00, 0x33, 0xb3, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x03, 0x0c, 0x00, 0xc0, 0x00, 0x01, 0x80, 0x00, 0x00, 0x31, 0xb3, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x07, 0x87, 0x8f, 0x80, 0x00, 0x01, 0x80, 0x00, 0x00, 0x31, 0x9e, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t YES_no[] ={ // 'YES-no', 128x8px
    0xff, 0xff, 0xf3, 0x3f, 0xff, 0xff, 0xff, 0xfe, 0x80, 0x00, 0x00, 0x31, 0x80, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xf3, 0x3f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x39, 0x80, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xf3, 0x38, 0x78, 0x3f, 0xff, 0xfe, 0x80, 0x00, 0x00, 0x3d, 0x9e, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xf8, 0x73, 0x33, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x37, 0xb3, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xfc, 0xf0, 0x38, 0x7f, 0xff, 0xfe, 0x80, 0x00, 0x00, 0x33, 0xb3, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xfc, 0xf3, 0xff, 0x3f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x31, 0xb3, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xf8, 0x78, 0x70, 0x7f, 0xff, 0xfe, 0x80, 0x00, 0x00, 0x31, 0x9e, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ssd1306_clear_screen(devPtr, false);
    ssd1306_display_text(devPtr, 0, row0,sizeof(row0)-1, false);
    ssd1306_display_text(devPtr, 1, row1,sizeof(row1)-1, false);
    ssd1306_display_text(devPtr, 2, locName,sizeof(locName)-1, false);
    ssd1306_bitmaps(devPtr, 0, 55, yes_no, 128, 8, false);
    for (;;)
    {
        if (xQueueReceive(keyEventQueue, &action, pdMS_TO_TICKS(30000)) == pdTRUE)
        {
            switch (action)
            {
            case ACTION_UP:
                ssd1306_display_text(devPtr, 2, locName, sizeof(locName) - 1, true);
                ssd1306_bitmaps(devPtr, 0, 55, yes_no, 128, 8, false);
                break;
            case ACTION_DOWN:
            case ACTION_LEFT:
                ssd1306_display_text(devPtr, 2, locName, sizeof(locName) - 1, false);
                ssd1306_bitmaps(devPtr, 0, 55, YES_no, 128, 8, false);
                break;

            case ACTION_RIGHT:
                ssd1306_display_text(devPtr, 2, locName, sizeof(locName) - 1, false);
                ssd1306_bitmaps(devPtr, 0, 55, YES_no, 128, 8, true);
                break;

            case ACTION_ENTER:
                return;
                break;

            default:
                break;
            }
        }
        else
        return;
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
    case KEY_BACK:
        if (keypad.bufferPos >= 0)
        {
            keypad.textBuffer[keypad.bufferPos] = '\0';
            keypad.bufferPos--;
            keypad.pendingChar = '\0';
        }
        break;
    case KEY_SHIFT:
        keypad.letterIsCapital = !keypad.letterIsCapital;
        break;

    default:
        if (pressedButton >= KEY_0 && pressedButton <= KEY_9)
        {

            if (keypad.lastPressedButton == pressedButton &&
                (currentTick - keypad.lastTick) < (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL)) // 1 second
            {
                // Same button pressed again within timeout: cycle character
                keypad.currentPressCount = (keypad.currentPressCount + 1) % keyMapLen[pressedButton];
            }
            else
            {
                // Different button, or timeout expired for the previous one, or first press in a sequence
                if (keypad.lastPressedButton != -1 && keypad.pendingChar != '\0')
                {
                    // Commit the previously pending character
                    if (keypad.bufferPos < keypad.bufferSize - 1)
                    {
                        // Check if pendingChar is letter and letterIsCapital set
                        if (keypad.pendingChar > 0x60 && keypad.pendingChar < 0x7a && keypad.letterIsCapital)
                            keypad.pendingChar -= 0x20; // Set as capital letter
                        keypad.bufferPos++;
                        keypad.textBuffer[keypad.bufferPos] = keypad.pendingChar;
                        keypad.textBuffer[keypad.bufferPos + 1] = '\0'; // Null-terminate
                        keypad.letterIsCapital = false;
                    }
                    // ESP_LOGI(TAG, "Committed: '%c'. Buffer: \"%s\"", pendingChar, textBuffer);
                }
                // Start new character sequence for the currently pressed button
                keypad.currentPressCount = 0;
                keypad.lastPressedButton = pressedButton;
            }
            keypad.pendingChar = keyMap[pressedButton][keypad.currentPressCount];
            if (keypad.pendingChar >= 'a' && keypad.pendingChar <= 'z' && keypad.letterIsCapital)
                keypad.pendingChar -= 0x20; // Set as capital letter
            keypad.textBuffer[keypad.bufferPos] = keypad.pendingChar;

            esp_err_t err = esp_timer_start_once(confirmation_timer_handle, 1000 * 1000ULL);
            if (err != ESP_OK)
            {
                // ESP_LOGE(TAG, "Failed to start confirmation timer: %s", esp_err_to_name(err));
            }
            break;
        }
    }
    keypad.lastTick = currentTick;
    char event = EVT_KEYPAD_PRESS;
    xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
}

// Timer callback to confirm the pending character if no other key is pressed
void confirmation_timer_callback(void *arg)
{   
    if (keypad.textBuffer == NULL || keypad.bufferSize == 0) 
        return;

    if (keypad.pendingChar != '\0' && keypad.lastPressedButton != -1)
    {
        if (keypad.bufferPos < keypad.bufferSize - 1)
        {
            if (keypad.pendingChar > 0x60 && keypad.pendingChar < 0x7a && keypad.letterIsCapital)
                keypad.pendingChar -= 0x20; // Set as capital letter

            keypad.bufferPos++;
            keypad.textBuffer[keypad.bufferPos] = keypad.pendingChar;
            keypad.textBuffer[keypad.bufferPos + 1] = '\0'; // Null-terminate
            keypad.letterIsCapital = false;
        }
        // ESP_LOGI(TAG, "Confirmed by timer: '%c'. Buffer: \"%s\"", pendingChar, textBuffer);

        keypad.pendingChar = '\0';
        keypad.lastPressedButton = -1; // Reset for next new key sequence
        keypad.currentPressCount = 0;
    }
    char event = EVT_KEYPAD_PRESS;
    xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));    
}

void stack_push(menu_t* state) {
    if (stack_top < MENU_STACK_SIZE - 1) {
        menu_stack[++stack_top] = state;
    }
}

menu_t* stack_pop() {
    if (stack_top > -1) {
        return menu_stack[stack_top--];
    }
    return NULL; // Stack was empty
}



// char* mainMenuList[] = {"Scan tag", "Transmit tag", "test", "test111"};
menu_t* currentMenu = &mainMenu;

void ui_handler_task(void* args)
{
    const char* TAGUI = "ui_handle";
    char event;      
    currentMenu->draw_func();
    
    for (;;)
    {        
        if (xQueueReceive(uiEventQueue, &event, pdMS_TO_TICKS(30000)) == pdTRUE)
        {            
            ESP_LOGI("ui_handler","Event: %d\tMenu: %d", event, currentMenu->menuWindow);
            // Allow back navigation from any state
            if (event == KEY_BACK)
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
            }
        }
    }
    
}

void display_delay_timer_callback()
{
    char event = EVT_NEXT_MENU;
    xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
}