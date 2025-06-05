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




void list_test()
{
    // Test display scrolling on button
        const char* list[] = {"zero","first","second","third","forth","fifth","sixth","seventh","eigth" ,"nineth", "tenth", "eleventh","EEEEOOOO"};        
        const uint8_t displayRows = 7, listSize= sizeof(list)/sizeof(&list);  
        unsigned int str_size[listSize];
        for (int i = 0; i < listSize; i++)
        {
            str_size[i] = strlen(list[i]);            
        }

        static int pos = 0, frame_top_pos = 0;
        pos++;
        if (pos > listSize - 1)
            pos = 0;
        if (pos > frame_top_pos + displayRows)
        {
            frame_top_pos = pos - displayRows;
            ssd1306_clear_screen(devPtr, false);
        }
        else if (pos < frame_top_pos)
        {
            frame_top_pos = pos;
            ssd1306_clear_screen(devPtr, false);
        }

        // ssd1306_clear_screen(&devPtr, false);
        for (int i = 0; i <= displayRows; i++)
        {            
            ssd1306_display_text(devPtr, i, list[i+frame_top_pos], str_size[i+frame_top_pos], (i+frame_top_pos) == pos ? 1 : 0);
        }
}

void display_menu_list(menu_t* menu, char event)
{
    const int maxPages = devPtr->_pages;
    char buffer[17];
    uint8_t listSize = menu->listSize;
    if (menu->startPage + menu->maxPages > maxPages)
    {       
        menu->maxPages = maxPages - menu->startPage;
        ESP_LOGE("menu_list","startPage + maxPages > maxPages; max pages = %d",menu->maxPages);
    }
        
    for (int page = 0; page < (menu->maxPages) && (page < listSize); page++)
    {
        int idx = page + menu->topRowIdx;
        int len = strlen(menu->list[idx]);
        strncpy(buffer, menu->list[idx], len);        

        if (len < sizeof(buffer)-1)         
            memset(buffer + len, 0, sizeof(buffer) - len - 1);

        ssd1306_display_text(devPtr, page + menu->startPage, buffer, 16, idx == menu->selectedRow ? 1 : 0);
    
    }
    
}

void display_scan_tag(menu_t* menu, char event)
{
    ssd1306_clear_screen(devPtr, false);
    int startPage = menu->startPage;    
    if (event == EVT_RFID_SCAN_DONE)
    {       
        char str[17];
        sprintf(str, "0x%010" PRIX64, currentTag);
        ssd1306_display_text(devPtr, startPage, "Scan succesfull", 16, true);
        ssd1306_display_text(devPtr, startPage + 1, str, 16, false);
    }
    else
    {
        ssd1306_display_text(devPtr, startPage, "Scanning tag   ", 16, false);
    }
}

void display_wifi_scan(menu_t* menu, char event)
{
    const char* TAG_WIFI_SCAN = "wifi_scan";
    
    ssd1306_clear_screen(devPtr, false);
    int startPage = menu->startPage;   
    if (event == EVT_WIFI_SCAN_DONE)
    {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        ESP_LOGI(TAG_WIFI_SCAN, "Total APs scanned = %d", ap_count);
        for (int i = 0; i < ap_count; i++)
        {
            ESP_LOGI(TAG_WIFI_SCAN, "SSID \t\t%s", ap_records[i].ssid);
            ESP_LOGI(TAG_WIFI_SCAN, "BSSID \t\t%02x:%02x:%02x:%02x:%02x:%02x", ap_records[i].bssid[0],
                     ap_records[i].bssid[1],ap_records[i].bssid[2],ap_records[i].bssid[3],
                     ap_records[i].bssid[4],ap_records[i].bssid[5]);
            ESP_LOGI(TAG_WIFI_SCAN, "RSSI \t\t%d", ap_records[i].rssi);
            ESP_LOGI(TAG_WIFI_SCAN, "Authmode \t%d", ap_records[i].authmode);
        }
        ssd1306_display_wifi_aps(ap_records,ap_count, startPage);
        free(ap_records);
    }
    else
    {
        ssd1306_display_text(devPtr, startPage, "Scanning WiFi  ", 16, false);
    }
    
}

void display_save_tag_prompt(menu_t* menu, char event)
{
    const char* TAG_SAVE_PROMPT = "save_prompt";
    ssd1306_clear_screen(devPtr, false);

}


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

static char* textBuffer  = NULL;
static int8_t bufferSize = 0;
static int8_t bufferPos = 0;
static char pendingChar = '\0'; // Character currently being formed by multi-presses
static uint8_t letterIsCapital = 0;
static int8_t lastPressedButton = -1; // Index of the last button pressed (0-8)
static int8_t currentPressCount = 0; // How many times the current button has been pressed in sequence
static int32_t lastTick = 0; // Timestamp of the last valid button press 
const uint8_t dislayCharWidth = 16;

void keypad_button_press(int8_t pressedButton, char *bufferPtr, uint8_t bufferSize)
{
    
    // char temp[20] = {0};
    textBuffer = bufferPtr;
    bufferSize = bufferSize;
    // ESP_LOGD(TAG, "Button %d (GPIO %d) processing", pressedButton + 1, button_gpios[pressedButton]);

    // Stop any pending confirmation timer, as a new action is happening
    esp_timer_stop(confirmation_timer_handle);
    int32_t currentTick = esp_cpu_get_cycle_count();

    switch (pressedButton)
    {
    case KEY_BACK:
        if (bufferPos >= 0)
        {
            textBuffer[bufferPos] = '\0';
            bufferPos--;            
            pendingChar = '\0';
        }
        break;
    case KEY_SHIFT:
        letterIsCapital = !letterIsCapital;
        break;    
    
    default:
        if (lastPressedButton == pressedButton && 
        (currentTick - lastTick) < (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL)) // 1 second
        {
            // Same button pressed again within timeout: cycle character
            currentPressCount = (currentPressCount + 1) % keyMapLen[pressedButton];
        }
        else
        {
            // Different button, or timeout expired for the previous one, or first press in a sequence
            if (lastPressedButton != -1 && pendingChar != '\0')
            {
                // Commit the previously pending character
                if (bufferPos < bufferSize - 1)
                {
                    // Check if pendingChar is letter and letterIsCapital set
                    if (pendingChar > 0x60 && pendingChar < 0x7a && letterIsCapital)
                        pendingChar -= 0x20; // Set as capital letter                    
                    bufferPos++;
                    textBuffer[bufferPos] = pendingChar;
                    textBuffer[bufferPos + 1] = '\0'; // Null-terminate
                    letterIsCapital = false;
                }
                // ESP_LOGI(TAG, "Committed: '%c'. Buffer: \"%s\"", pendingChar, textBuffer);
            }
            // Start new character sequence for the currently pressed button
            currentPressCount = 0;
            lastPressedButton = pressedButton;
        }
        pendingChar = keyMap[pressedButton][currentPressCount];
        if (pendingChar >= 'a' && pendingChar <= 'z' && letterIsCapital)
            pendingChar -= 0x20; // Set as capital letter
        textBuffer[bufferPos] = pendingChar;

        esp_err_t err = esp_timer_start_once(confirmation_timer_handle, 1000 * 1000ULL);
        if (err != ESP_OK)
        {
            // ESP_LOGE(TAG, "Failed to start confirmation timer: %s", esp_err_to_name(err));
        }
        break;
    }      
    lastTick = currentTick;

    // sprintf(temp, "pc=%c bp=%d pb=%d", pendingChar, bufferPos, pressedButton);
    // ssd1306_display_text(devPtr, 0, temp, 16, false);
    // sprintf(temp, "PrCnt=%d lpb=%d", currentPressCount, lastPressedButton);
    // ssd1306_display_text(devPtr, 1, temp, 16, false);
    ssd1306_display_text(devPtr, currentMenu.textFieldPage, bufferPos < dislayCharWidth ? textBuffer : textBuffer + (bufferPos - dislayCharWidth + 1), dislayCharWidth, false);

}

// Timer callback to confirm the pending character if no other key is pressed
void confirmation_timer_callback(void *arg)
{   
    if (textBuffer == NULL || bufferSize == 0) 
        return;

    if (pendingChar != '\0' && lastPressedButton != -1)
    {
        if (bufferPos < bufferSize - 1)
        {
            if (pendingChar > 0x60 && pendingChar < 0x7a && letterIsCapital)
                pendingChar -= 0x20; // Set as capital letter

            bufferPos++;
            textBuffer[bufferPos] = pendingChar;
            textBuffer[bufferPos + 1] = '\0'; // Null-terminate
            letterIsCapital = false;
        }
        // ESP_LOGI(TAG, "Confirmed by timer: '%c'. Buffer: \"%s\"", pendingChar, textBuffer);

        pendingChar = '\0';
        lastPressedButton = -1; // Reset for next new key sequence
        currentPressCount = 0;
    }
    ssd1306_display_text(devPtr, currentMenu.textFieldPage, bufferPos < dislayCharWidth ? textBuffer : textBuffer + (bufferPos - dislayCharWidth + 1), dislayCharWidth, false);
}

int8_t is_stack_full(menu_stack_t *stack)
{
    if (stack->top >= MAX_SIZE - 1)
        return true;
    else
        return false;
}
int8_t is_stack_empty(menu_stack_t *stack)
{
    if (stack->top == -1)
        return true;
    else
        return false;
}
int8_t stack_push(menu_stack_t* stack, menu_t* menu)
{
    if (is_stack_full(stack))
        return PUSH_ERROR;
    stack->top += 1;
    stack->menus[stack->top] = menu;
    return 0;
}
menu_t* stack_pop(menu_stack_t* stack)
{
    if (is_stack_empty(stack))
        return NULL;
    menu_t *menu = stack->menus[stack->top];    
    stack->top -= 1;
    return menu;
}

char* mainMenuList[] = {"Transmit tag", "Read tag", "test", "test111", "t", "test", "test3333333", "abcdefg"};
// char* mainMenuList[] = {"Scan tag", "Transmit tag", "test", "test111"};

menu_t currentMenu = {
    .menuWindow = MAIN_MENU,
    .list = mainMenuList,
    .listSize = 8,
    .selectedRow = 0,
    .startPage = 4,
    .maxPages = 7,
    .displayFunc = display_menu_list,
};
menu_stack_t menuStack = {.top = -1,};

void ui_handler_task(void* args)
{
    const char* TAGUI = "ui_handle";
    char event;
    char textBuffer[20] = {0};        
    currentMenu.displayFunc(&currentMenu, 0);
    
    for (;;)
    {
        
        if (xQueueReceive(uiEventQueue, &event, pdMS_TO_TICKS(30000)) == pdTRUE)
        {
            char funcData = 0;
            ESP_LOGI("ui_handler", "Key: %d", event);
            switch (currentMenu.menuWindow)
            {
            case MAIN_MENU:
                if (event == KEY_UP)
                {
                    currentMenu.selectedRow--;
                    // Wrap-around to bottom if we go above first item
                    if (currentMenu.selectedRow < 0)
                        currentMenu.selectedRow = currentMenu.listSize - 1;                                      
                } 
                else if (event == KEY_DOWN)
                {
                    currentMenu.selectedRow++;
                    // Wrap-around to top if we go past last item                   
                    if (currentMenu.selectedRow > currentMenu.listSize - 1)
                        currentMenu.selectedRow = 0;
                }
                // Adjust the top row index for scrolling:
                // If selected row moves below the visible page, scroll down
                if (currentMenu.selectedRow >= currentMenu.topRowIdx + currentMenu.maxPages )                
                    currentMenu.topRowIdx = currentMenu.selectedRow - (currentMenu.maxPages -1);
                 // If selected row moves above the visible page, scroll up
                else if (currentMenu.selectedRow < currentMenu.topRowIdx)
                    currentMenu.topRowIdx = currentMenu.selectedRow;                                    
                // display_menu_list(&currentMenu);

                if (event == KEY_ENTER)
                {
                    // Allocate memory to copy currentMenu and save pointer to it in stack
                    menu_t* menuPtr = malloc(sizeof(menu_stack_t));
                    if (menuPtr == NULL)
                        ESP_LOGE(TAGUI, "malloc failed");
                    else
                        memcpy(menuPtr, &currentMenu, sizeof(menu_t));
                    if (stack_push(&menuStack, menuPtr) == PUSH_ERROR)
                        ESP_LOGE(TAGUI, "malloc failed");
                    switch (currentMenu.selectedRow)
                    {
                    case 0: // Receive tag choice
                        ESP_LOGI(TAGUI, "enter to receive");
                        memset(&currentMenu, 0, sizeof(menu_t));
                        currentMenu.menuWindow = TAG_SCAN;
                        currentMenu.displayFunc = display_scan_tag;
                        enable_read_tag();                       
                        break;

                    default:
                        break;
                    }                   
                } 
                break;

            case TAG_SCAN:
                if (event == EVT_RFID_SCAN_DONE)   
                {             
                    funcData = event; // Send event data to dislapy callback
                    // Delay timer to show access points
                    esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAGUI, "Failed to create delay timer: %s", esp_err_to_name(err));
                        break;
                    }
                }
                else if (event == KEY_BACK)
                {
                    // Get pointer of main menu in stack, copy memory to currentMenu and free the pointer                    
                    menu_t* menuPtr = stack_pop(&menuStack);
                    if (menuPtr == NULL)
                    {
                        ESP_LOGI(TAGUI, "TAG_SCAN stack pop failed");
                        continue;
                    }
                    memcpy(&currentMenu, menuPtr, sizeof(menu_t));
                    free(menuPtr);
                    disable_rx_tx_tag();
                }
                else if (event == EVT_NEXT_MENU)
                {
                    currentMenu.menuWindow = WIFI_SCAN;
                    currentMenu.displayFunc = display_wifi_scan;
                    wifi_scan_config_t scan_cfg = {
                        .ssid = NULL,       // scan for all SSIDs
                        .bssid = NULL,      // scan for all BSSIDs
                        .channel = 0,       // 0 = scan all channels
                        .show_hidden = true // include hidden SSIDs
                    };
                    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAGUI, "esp_wifi_scan_start: %s", esp_err_to_name(err));
                        break;
                    }
                }
                break;

            case WIFI_SCAN:
                if (event == EVT_WIFI_SCAN_DONE)
                {
                    funcData = event;
                    esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAGUI, "Failed to create delay timer: %s", esp_err_to_name(err));
                        break;
                    }
                }                
                else if (event == KEY_BACK)
                {
                    menu_t* menuPtr = stack_pop(&menuStack);
                    if (menuPtr == NULL)
                    {
                        ESP_LOGI(TAGUI, "TAG_SCAN stack pop failed");
                        continue;
                    }
                    memcpy(&currentMenu, menuPtr, sizeof(menu_t));
                    free(menuPtr);
                }
                else if (event == EVT_NEXT_MENU)
                {
                    currentMenu.menuWindow = SAVE_TAG_PROMPT;
                    // currentMenu.displayFunc = display_save_tag_prompt;
                    currentMenu.displayFunc = NULL;
                    currentMenu.textFieldPage = 2;
                    ssd1306_clear_screen(devPtr, false);
                    ssd1306_display_text(devPtr, 1, "Location name: ", 16, false);
                }
                break;

            case SAVE_TAG_PROMPT:
                if (event >= KEY_0 && event <= KEY_SHIFT)
                {
                    keypad_button_press(event, textBuffer, sizeof(textBuffer));
                }

                break;
            default:
                break;
            }
            if (currentMenu.displayFunc != NULL)
                currentMenu.displayFunc(&currentMenu, funcData);
        }
    }
    
}

void display_delay_timer_callback()
{
    char event = EVT_NEXT_MENU;
    xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
}