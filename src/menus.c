#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rfid.h"
#include "menus.h"
#include "bitmaps.h"
#include "littlefs_records.h"
#include <u8g2.h>
#include "globals_menus.h"

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

menu_t* go_to_main_menu()
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

void display_list(menu_t* menu) 
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

void list_event_handle(menu_t *menu, int32_t event)
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