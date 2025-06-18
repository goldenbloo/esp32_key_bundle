// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "string.h"
// #include "esp_wifi.h"
// #include "macros.h"
// #include "esp_timer.h"
// #include "esp_log.h"
// #include "rfid.h"
// // #include "menus.h"
// // #include "ssd1306.h"
// #include "littlefs_records.h"

// // Wifi records----------------------------------------------------------------
// static wifi_ap_record_t ap_records[BSSID_MAX];
// static uint16_t ap_count = 0;
// // Found best locations--------------------------------------------------------
// location_t bestLocs[LOC_MATCH_MAX];
// uint32_t locNum;
// // Menu stack------------------------------------------------------------------
// static menu_t* menuStack[MENU_STACK_SIZE];
// int stack_top = -1;
// void stack_push(menu_t* state) 
// {
//     if (stack_top < MENU_STACK_SIZE - 1) {
//         menuStack[++stack_top] = state;
//     }
// }
// menu_t* stack_pop() 
// {
//     if (stack_top > -1) {
//         return menuStack[stack_top--];
//     }
//     return NULL; // Stack was empty
// }
// //Keypad struct and text field buffer------------------------------------------
// char fieldBuffer[FIELD_SIZE];
// keypad_t keypad = {
// .textBuffer = fieldBuffer,
// .bufferSize = sizeof(fieldBuffer),
// .bufferPos = -1,
// .lastPressedButton = -1,
// .displayCharWidth= 16,
// };
// //Menus functions--------------------------------------------------------------
// static void display_list(menu_t *menu);
// static menu_t* go_to_main_menu();

// static menu_t *main_menu_handle(int32_t event);
// static void main_menu_draw();
// static void main_menu_exit();

// static void scan_tag_menu_enter();
// static menu_t *scan_tag_menu_handle(int32_t event);
// static void scan_tag_menu_exit();
// static void scan_tag_menu_draw();

// static void scan_wifi_menu_enter();
// static menu_t *scan_wifi_menu_handle(int32_t event);
// static void scan_wifi_menu_exit();
// static void scan_wifi_menu_draw();

// static void save_tag_menu_enter();
// static menu_t *save_tag_menu_handle(int32_t event);
// static void save_tag_menu_draw();

// static void transmit_menu_enter();
// static menu_t* transmit_menu_handler(int32_t event);
// static void transmit_menu_exit();
// static void transmit_menu_draw();


// //Menus structs----------------------------------------------------------------
// char* mainMenuEntries[] = {"Read tag", "Transmit tag", "Dump records", "Delete all tags", "t", "test", "test3333333", "abcdefg"};
// menu_listbox_t mainMenuListBox = {
//     .list = mainMenuEntries,
//     .listSize = 8,
//     .selectedRow = 0,
// };
// char *locNameList[LOC_MATCH_MAX + 1];
// menu_listbox_t transmitMenuListBox = {
//     .selectedRow = 0,
//     .list = locNameList,
// };
// menu_textbox_t saveTagMenuTextBox = {
//     .textFieldBuffer = fieldBuffer,
//     .textFieldBufferSize = FIELD_SIZE,
//     .textFieldPage = 3,
// };
// //-----------------------------------------------------------------------------
// menu_t mainMenu = {
//     .menuName = MAIN_MENU,
//     .listBox = &mainMenuListBox,
//     .startPage = 1,
//     .maxPages = 7,
//     .draw_func = main_menu_draw,
//     .event_handler_func = main_menu_handle,
//     .exit_func = main_menu_exit,
// };
// menu_t scanTagMenu = {
//     .menuName = TAG_SCAN,    
//     .startPage = 1,
//     .maxPages = 7,
//     .draw_func = scan_tag_menu_draw,
//     .event_handler_func = scan_tag_menu_handle,
//     .enter_func = scan_tag_menu_enter,
//     .exit_func = scan_tag_menu_exit,
//     .back_handler_func = go_to_main_menu,
// };
// menu_t scanWifiMenu = {
//     .menuName = WIFI_SCAN,    
//     .startPage = 1,
//     .maxPages = 7,
//     .draw_func = scan_wifi_menu_draw,
//     .enter_func = scan_wifi_menu_enter,
//     .event_handler_func = scan_wifi_menu_handle,
//     .exit_func = scan_wifi_menu_exit,
//     .back_handler_func = go_to_main_menu, 

// };
// menu_t saveTagMenu = {
//     .menuName = SAVE_TAG_MENU,    
//     .startPage = 1,
//     .maxPages = 7,
//     .textBox = &saveTagMenuTextBox,
//     .draw_func = save_tag_menu_draw,
//     .event_handler_func = save_tag_menu_handle,
//     .enter_func = save_tag_menu_enter,
//     .back_handler_func = go_to_main_menu,
   
// };
// menu_t transmitMenu = {
//     .menuName = TRANSMIT_TAG_MENU,
//     .listBox = &transmitMenuListBox,
//     .startPage = 1,
//     .maxPages = 7,
//     .draw_func = transmit_menu_draw,
//     .enter_func = transmit_menu_enter,
//     .event_handler_func = transmit_menu_handler,
//     .exit_func = transmit_menu_exit,
//     .back_handler_func = go_to_main_menu,
    
    
// };

// //===================================================================
// static menu_t* main_menu_handle(int32_t event)
// {
//     const char* TAG = "main_menu_handle";
//     if (mainMenu.listBox == NULL)
//     {
//         // ESP_LOGI(TAG, "Main Menu have no textbox");
//         return NULL;
//     }
//     if (event == KEY_UP)
//     {
//         mainMenu.listBox->selectedRow--;
        
//         // Wrap-around to bottom if we go above first item
//         if (mainMenu.listBox->selectedRow < 0)
//             mainMenu.listBox->selectedRow = mainMenu.listBox->listSize - 1;
//     }
//     else if (event == KEY_DOWN)
//     {
//         mainMenu.listBox->selectedRow++;
//         // Wrap-around to top if we go past last item
//         if (mainMenu.listBox->selectedRow > mainMenu.listBox->listSize - 1)
//             mainMenu.listBox->selectedRow = 0;
//     }
//     // Adjust the top row index for scrolling:
//     // If selected row moves below the visible page, scroll down
//     if (mainMenu.listBox->selectedRow >= mainMenu.listBox->topRowIdx + mainMenu.maxPages)
//         mainMenu.listBox->topRowIdx = mainMenu.listBox->selectedRow - (mainMenu.maxPages - 1);
//     // If selected row moves above the visible page, scroll up
//     else if (mainMenu.listBox->selectedRow < mainMenu.listBox->topRowIdx)
//         mainMenu.listBox->topRowIdx = mainMenu.listBox->selectedRow;
//     // display_list(&mainMenu);

//     if (event == KEY_ENTER)
//     {       
//         switch (mainMenu.listBox->selectedRow)
//         {
//         case 0: // Receive tag choice
//             ESP_LOGI(TAG, "enter to receive");
//             return &scanTagMenu;
//             break;        

//         case 1: // Transmit choice            
//             ESP_LOGI(TAG, "enter to trasmit");
//             scanWifiMenu.nextMenu = &transmitMenu;
//             return &scanWifiMenu;            
//             break;

//         case 2: // Dump locations choice            
//             read_all_locations();
//             break;

//         case 3: // Delete all locations choice            
//             clear_all_locations();
//             break;

//         default:
//             break;
//         }
//     }
//     return NULL;
// }

// static void main_menu_draw()
// {
//     display_list(&mainMenu);
// }

// static void main_menu_exit()
// {
//     // ssd1306_clear_screen(devPtr, false);
// }

// //===================================================================
// static void scan_tag_menu_enter()
// {
//     scanTagMenu.status = EVT_ON_ENTRY;    
//      rfid_enable_rx_tag();
// }

// static menu_t* scan_tag_menu_handle(int32_t event)
// {
//     if (scanTagMenu.status == EVT_ON_ENTRY) scanTagMenu.status = 0;
//     const char* TAG = "scan_tag_handle";
//     if (event == EVT_RFID_SCAN_DONE)
//     {
//         scanTagMenu.status = event; // Send event data to dislapy callback
//         // Delay timer to show access points
//         esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
//         if (err != ESP_OK)        
//             ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));        
//     }
//     else if (event == EVT_NEXT_MENU)
//     {
//         scanWifiMenu.nextMenu = &saveTagMenu;
//         return &scanWifiMenu;
//     }
//     return NULL;
// }

// static void scan_tag_menu_exit()
// {
//     rfid_disable_rx_tx_tag();
//     esp_timer_stop(display_delay_timer_handle);
// }

// static void scan_tag_menu_draw()
// {
//     ssd1306_clear_screen(devPtr, false);
//     int startPage = scanTagMenu.startPage;    
//     if (scanTagMenu.status == EVT_RFID_SCAN_DONE)
//     {       
//         char str[17] = {0};
//         sprintf(str, "0x%010llX", currentTag);
//         ssd1306_display_text(devPtr, startPage, "Scan succesfull", 16, true);
//         ssd1306_display_text(devPtr, startPage + 2, str, 16, false);
//     }
//     else if (scanTagMenu.status == EVT_ON_ENTRY)
//     {
//         ssd1306_display_text(devPtr, scanTagMenu.startPage, "Scanning tag   ", 16, false);
//     }    
// }

// //===================================================================
// static void scan_wifi_menu_enter()
// {
//     const char *TAG = "wifi_enter";
//     wifi_scan_config_t scan_cfg = {
//         .ssid = NULL,       // scan for all SSIDs
//         .bssid = NULL,      // scan for all BSSIDs
//         .channel = 0,       // 0 = scan all channels
//         .show_hidden = true // include hidden SSIDs
//     };   
//     esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
//     if (err != ESP_OK)
//         ESP_LOGE(TAG, "esp_wifi_scan_start: %s", esp_err_to_name(err));
//     scanWifiMenu.status = EVT_ON_ENTRY;    
// }

// static menu_t* scan_wifi_menu_handle(int32_t event)
// {
//     const char* TAG = "wifi_handle";
//     if (scanWifiMenu.status == EVT_ON_ENTRY) scanWifiMenu.status = 0;
//     if (event == EVT_WIFI_SCAN_DONE)
//     {
//         scanWifiMenu.status = event;
//         esp_wifi_scan_get_ap_num(&ap_count);
//         if (ap_count > BSSID_MAX) ap_count = BSSID_MAX;
//         esp_wifi_scan_get_ap_records(&ap_count, ap_records);

//         esp_err_t err = esp_timer_start_once(display_delay_timer_handle, 2000 * 1000ULL);
//         if (err != ESP_OK)        
//             ESP_LOGE(TAG, "Failed to create delay timer: %s", esp_err_to_name(err));         
//     }    
//     else if (event == EVT_NEXT_MENU)
//     {
//         if (scanWifiMenu.nextMenu != NULL)
//             return scanWifiMenu.nextMenu;
//         else
//             return &saveTagMenu;
//     }
//     return NULL;
// }

// static void scan_wifi_menu_exit()
// {
//     esp_timer_stop(display_delay_timer_handle);
// }

// static void scan_wifi_menu_draw()
// {
//     // const char* TAG_WIFI_SCAN = "wifi_scan";    
    
//     if (scanWifiMenu.status == EVT_WIFI_SCAN_DONE)
//     {
//         ssd1306_clear_screen(devPtr, false);
//         ssd1306_display_wifi_aps(ap_records,ap_count, scanWifiMenu.startPage);
//         // free(ap_records);
//     } 
//     else if (scanWifiMenu.status == EVT_ON_ENTRY) 
//     {
//         ssd1306_clear_screen(devPtr, false);          
//         ssd1306_display_text(devPtr, scanWifiMenu.startPage, "Scanning WiFi  ", 16, false);
//     }
// }

// //===================================================================
// static void save_tag_menu_enter()
// {        
//     if (saveTagMenu.textBox == NULL){
//         ESP_LOGE("wifi_enter", "Textbox in saveTagMenu in NULL"); 
//         return;
//     }
//     memset(saveTagMenu.textBox->textFieldBuffer, 0, saveTagMenu.textBox->textFieldBufferSize); 
//     keypad.bufferPos = -1;   
//     keypad.lastPressedButton = -1;
//     keypad.displayCharWidth = 16;
//     saveTagMenu.status = EVT_ON_ENTRY;
//     saveTagMenu.selectedOption = NO_OPTION;
// }

// static menu_t* save_tag_menu_handle(int32_t event)
// {
//     if (saveTagMenu.status == EVT_ON_ENTRY) saveTagMenu.status = 0;
//     switch (event)
//     {
//     case EVT_KEYPAD_PRESS:
//         saveTagMenu.status = EVT_KEYPAD_PRESS;
//         break;

//     case KEY_LEFT:
//         saveTagMenu.selectedOption = SAVE_OPTION;
//         saveTagMenu.status = 0;
//         break;

//     case KEY_RIGHT:
//         saveTagMenu.selectedOption = CANCEL_OPTION;
//         saveTagMenu.status = 0;
//         break;

//     case KEY_ENTER:
//         switch (saveTagMenu.selectedOption)
//         {
//         case SAVE_OPTION:

//             int32_t currentLocId = get_next_location_id();
//             if (currentLocId >= 0)
//             {
//                 location_t loc = {.id = currentLocId};
//                 for (int i = 0; i < ap_count && i < BSSID_MAX; i++)
//                 {
//                     memcpy(loc.bssids[i], ap_records[i].bssid, sizeof(loc.bssids[i]));
//                     loc.rssis[i] = ap_records[i].rssi;
//                 }
//                 for (int i = ap_count; i < BSSID_MAX; i++)
//                 {
//                     memset(loc.bssids[i], 0, sizeof(loc.bssids[i]));
//                     loc.rssis[i] = 0;
//                 }
//                 strncpy(loc.name, saveTagMenuTextBox.textFieldBuffer, sizeof(loc.name));

//                 memcpy(loc.tag, currentTagArray, sizeof(loc.tag));
//                 append_location(&loc);
//             }            
//             // Fall through to CANCEL_OPTION to return to main menu           
//             // break;

//         case CANCEL_OPTION:
//          uint32_t backEvent = KEY_BACK;
//             xQueueSendToBack(uiEventQueue, &backEvent, pdMS_TO_TICKS(15));
//             break;
        
//         default:
//             break;
//         }

//         break;

//     default:
//         if (event >= KEY_0 && event <= KEY_SHIFT)
//         {
//             keypad_button_press(event);
//             saveTagMenu.status = EVT_KEYPAD_PRESS;            
//         }
//         break;
//     }
//     return NULL;
// }

// static void save_tag_menu_draw()
// {
//     switch (saveTagMenu.status)
//     {
//     case EVT_KEYPAD_PRESS:
//        ssd1306_display_text(devPtr,
//                              saveTagMenu.textBox->textFieldPage,
//                              keypad.bufferPos < keypad.displayCharWidth ? keypad.textBuffer : keypad.textBuffer + (keypad.bufferPos - keypad.displayCharWidth + 1),
//                              keypad.displayCharWidth, false);
//         break;
    
//     case EVT_ON_ENTRY:
//         ssd1306_clear_screen(devPtr, false);
//         ssd1306_display_text(devPtr, saveTagMenu.textBox->textFieldPage - 1, "Location name: ", 16, false);        
//         ssd1306_bitmaps(devPtr,0,56,save_cancel_unchecked_img,128,8,false);
//         break;

//     default:
//         switch (saveTagMenu.selectedOption)
//         {
//         case SAVE_OPTION:
//             ssd1306_bitmaps(devPtr,0,56,SAVE_cancel_img,128,8,false);            
//             break;
        
//         case CANCEL_OPTION:
//             ssd1306_bitmaps(devPtr,0,56,SAVE_cancel_img,128,8,true);
//             break;
        
//         case NOT_SELECTED:
//             ssd1306_bitmaps(devPtr,0,56,save_cancel_unchecked_img,128,8,false);
//             break;
        
//         default:
//             break;
//         }
//         break;
//     }

// }

// static void transmit_menu_enter()
// {
//     const char *TAG = "transmit_enter";
    
    
//     transmitMenu.status = EVT_ON_ENTRY;    

//     if (ap_count == 0)
//     {
//         ssd1306_clear_screen(devPtr, false);
//         ssd1306_display_text(devPtr, 0, "APs not found",14,false);
//         return;
//     }
    
//     uint8_t (*bssids)[6] = malloc(sizeof(*bssids) * ap_count);
//     int8_t* rssis = malloc(sizeof(int8_t) * ap_count);
//     if ((rssis == NULL) && (bssids == NULL))
//     {
//         ESP_LOGE("transmit_enter","Malloc failed");
//         return;
//     }
//     for (int i = 0; i < ap_count; i++)
//     {
//         memcpy(bssids[i], ap_records[i].bssid, sizeof(ap_records[i].bssid));
//         rssis[i] = ap_records[i].rssi;
//     }
//     locNum = find_multiple_best_locations(bssids,rssis,ap_count,bestLocs);
    
//     locNameList[0] = "Auto";
//     for (int i = 0; i < locNum; i++)
//     {
//         // char buff[17] = {0};
//         // ssd1306_clear_screen(devPtr, false);
//         // sprintf(buff,"Id: %ld", bestLocs[i].id);
//         // ssd1306_display_text(devPtr, 0, buff,16,false);
//         // ssd1306_display_text(devPtr, 1, bestLocs[i].name,16,false);
//         // sprintf(buff,"Tag:0x%010llX", rfid_array_to_tag(bestLocs[i].tag));
//         // ssd1306_display_text(devPtr, 2, buff,16,false);
//         // ssd1306_display_wifi_aps(ap_records, ap_count, 3);
//         // vTaskDelay(3000/portTICK_PERIOD_MS) ;
//         ESP_LOGI(TAG, "Id: %ld\nName: %s\nTag: 0x%010llX", bestLocs[i].id, bestLocs[i].name, rfid_array_to_tag(bestLocs[i].tag));
//         locNameList[i + 1] = bestLocs[i].name;
//     }

//     if (locNum == 0)
//     {
//         ssd1306_clear_screen(devPtr, false);
//         ssd1306_display_text(devPtr, 1, "Tag not found",14,false);
//     }
//     else
//     {        
//         transmitMenuListBox.listSize = locNum + 1;
//         transmitMenuListBox.selectedRow = 0;
//         transmitMenuListBox.topRowIdx = 0;
//     }
    
//     free(bssids);
//     free(rssis);
// }

// static menu_t* transmit_menu_handler(int32_t event)
// {
//     if (transmitMenu.status == EVT_ON_ENTRY) transmitMenu.status = 0;

//     if (transmitMenu.listBox == NULL) return NULL;    
//     if (event == KEY_UP)
//     {
//         transmitMenu.listBox->selectedRow--;
        
//         // Wrap-around to bottom if we go above first item
//         if (transmitMenu.listBox->selectedRow < 0)
//             transmitMenu.listBox->selectedRow = transmitMenu.listBox->listSize - 1;
//     }
//     else if (event == KEY_DOWN)
//     {
//         transmitMenu.listBox->selectedRow++;
//         // Wrap-around to top if we go past last item
//         if (transmitMenu.listBox->selectedRow > transmitMenu.listBox->listSize - 1)
//             transmitMenu.listBox->selectedRow = 0;
//     }
//     // Adjust the top row index for scrolling:
//     // If selected row moves below the visible page, scroll down
//     if (transmitMenu.listBox->selectedRow >= transmitMenu.listBox->topRowIdx + transmitMenu.maxPages)
//         transmitMenu.listBox->topRowIdx = transmitMenu.listBox->selectedRow - (transmitMenu.maxPages - 1);
//     // If selected row moves above the visible page, scroll up
//     else if (transmitMenu.listBox->selectedRow < transmitMenu.listBox->topRowIdx)
//         transmitMenu.listBox->topRowIdx = transmitMenu.listBox->selectedRow;

//     if (transmitMenuListBox.selectedRow == 0)    
//     {
//         vTaskResume(autoTxHandler);    
//         ESP_LOGI("transmit menu", "Row = 0");
//     }
//     else
//     {
//         vTaskSuspend(autoTxHandler);
//         rfid_enable_tx_raw_tag(rfid_array_to_tag(bestLocs[transmitMenuListBox.selectedRow - 1].tag));
//         ESP_LOGI("transmit menu", "Row = %d", transmitMenuListBox.selectedRow - 1);
//     }
//     return NULL;
// }

// void transmit_menu_exit()
// {
//     vTaskSuspend(autoTxHandler);    
//     rfid_disable_rx_tx_tag();
//     ESP_LOGI("transmit menu", "EXIT");
// }

// static void transmit_menu_draw()
// {
//     // ssd1306_display_text(devPtr, transmitMenu.startPage, "Transmit locat", 16, false);
//     ESP_LOGI("transmit menu", "Draw start");
//     display_list(&transmitMenu);
//     ESP_LOGI("transmit menu", "Draw End");
// }


// static menu_t* go_to_main_menu()
// {
//     menu_t *returnMenu = NULL;
//     while (1)
//     {
//         returnMenu = stack_pop();
//         if (returnMenu == NULL)
//             return NULL;
//         if (returnMenu->menuName == MAIN_MENU)
//             return returnMenu;
//     }
// }

// //===================================================================
// void ssd1306_display_wifi_aps(wifi_ap_record_t *ap_records, uint16_t ap_count, uint32_t startPage)
// {
//     const int maxPages = devPtr->_pages;
//     char str[20];
//     // ssd1306_clear_screen(devPtr, false);
//     for (uint8_t i = 0; i < 5 && i < ap_count && i < maxPages - startPage; i++)
//     {
//         sprintf(str, "%02x:%02x:%02x:%02x %d",
//                 ap_records[i].bssid[2],
//                 ap_records[i].bssid[3],
//                 ap_records[i].bssid[4],
//                 ap_records[i].bssid[5],
//                 ap_records[i].rssi);
//         ssd1306_display_text(devPtr, i + startPage, str, strlen(str), false);
//     }
// }

// static void display_list(menu_t* menu) 
// {
//     const int maxPages = devPtr->_pages;
//     char buffer[17];
//     uint8_t listSize = menu->listBox->listSize;
    
//     if (menu->startPage + menu->maxPages > maxPages)
//     {    
//         menu->maxPages = maxPages - menu->startPage;
//         ESP_LOGE("menu_list", "startPage + maxPages > maxPages; max pages = %d", menu->maxPages);
//     }
    
//     for (int page = 0; page < (menu->maxPages) && (page < listSize); page++)
//     {
//         int idx = page + menu->listBox->topRowIdx;
//         // Ensure you don't read past the end of the list if there's a logic error elsewhere
//         if (idx >= listSize) continue;        
        
//         strncpy(buffer, menu->listBox->list[idx], sizeof(buffer) - 1);
//         buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination
        
//         ssd1306_display_text(devPtr, page + menu->startPage, buffer, 16, idx == menu->listBox->selectedRow ? 1 : 0);
//     }
// }


// #define NUM_BUTTONS 10
// #define MAX_CHARS_PER_KEY 5
// const char keyMap[NUM_BUTTONS][MAX_CHARS_PER_KEY] = {
//     {' ', '0'},               // Key '0'
//     {'1', '.', ',', '!', '?'},// Key '1'
//     {'a', 'b', 'c', '2'},     // Key '2'
//     {'d', 'e', 'f', '3'},     // Key '3'
//     {'g', 'h', 'i', '4'},     // Key '4'
//     {'j', 'k', 'l', '5'},     // Key '5'
//     {'m', 'n', 'o', '6'},     // Key '6'
//     {'p', 'q', 'r', 's', '7'},// Key '7'
//     {'t', 'u', 'v', '8'},     // Key '8'
//     {'w', 'x', 'y', 'z', '9'} // Key '9'
// };

// const uint8_t keyMapLen[NUM_BUTTONS] = {
//     2, // For Key '0'
//     5, // For Key '1'
//     4, // For Key '2'
//     4, // For Key '3'
//     4, // For Key '4'
//     4, // For Key '5'
//     4, // For Key '6'
//     5, // For Key '7'
//     5, // For Key '8'
//     5  // For Key '9'
// };

// void keypad_button_press(int8_t pressedButton)
// {
//     const char* TAG = "keypad_button_press";
//     if (keypad.textBuffer == NULL || keypad.bufferSize < 1)
//     {
//         ESP_LOGE(TAG,"textBuffer is NULL or bufferSize < 1");
//         return;
//     }

//     // Stop any pending confirmation timer, as a new action is happening
//     esp_timer_stop(confirmation_timer_handle);
//     int32_t currentTick = esp_cpu_get_cycle_count();

//     switch (pressedButton)
//     {
//     case KEY_CLEAR_CHAR:
//         if (keypad.lastPressedButton != -1)
//         {
//             esp_timer_stop(confirmation_timer_handle); // cancel auto‐commit
//             keypad.lastPressedButton = -1;
//             keypad.currentPressCount = 0;
//             // remove the “pending” slot
//             int pendingIndex = keypad.bufferPos + 1;
//             keypad.textBuffer[pendingIndex] = '\0';
//         }
//         // Otherwise, backspace the last confirmed character (if any).
//         else if (keypad.bufferPos >= 0)
//         {
//             // remove the char at bufferPos
//             keypad.textBuffer[keypad.bufferPos] = '\0';
//             keypad.bufferPos--;
//         }
//         break;
        
//     case KEY_SHIFT:
//         keypad.letterIsCapital = !keypad.letterIsCapital;
//         break;

//     default:
//         if ((pressedButton >= KEY_0) && (pressedButton <= KEY_9))
//         {
//             // Check if this is a continuation of the current multi-tap sequence.
//             if (keypad.lastPressedButton == pressedButton &&
//                 (currentTick - keypad.lastTick) < (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL))
//             {
//                 // Cycle among the possible characters.
//                 keypad.currentPressCount = (keypad.currentPressCount + 1) % keyMapLen[pressedButton];
//             }
//             else
//             {
//                 // A new key or a timeout: commit any pending character if one exists.
//                 if (keypad.lastPressedButton != -1)
//                 {
//                     if (keypad.bufferPos < keypad.bufferSize - 2)                    
//                         keypad.bufferPos++; // Commit the pending character.                    
//                     else                    
//                         ESP_LOGW(TAG, "Buffer full, cannot commit pending character");                    
//                 }
//                 // Begin a new multi-tap sequence.
//                 keypad.lastPressedButton = pressedButton;
//                 keypad.currentPressCount = 0;
//             }
//             // Write/update the pending character at index (bufferPos+1) in the same buffer.
//             int pendingIndex = keypad.bufferPos + 1;
//             if (pendingIndex < keypad.bufferSize - 1)
//             { // Make sure there is room for the char and a null terminator.
//                 char newChar = keyMap[pressedButton][keypad.currentPressCount];
//                 if ((newChar >= 'a') && (newChar <= 'z') && keypad.letterIsCapital)
//                     newChar -= 0x20; // Capitalize if needed.
//                 keypad.textBuffer[pendingIndex] = newChar;
//                 keypad.textBuffer[pendingIndex + 1] = '\0';
//             }
//             else
//             {
//                 ESP_LOGW(TAG, "Buffer full, cannot add new pending character");
//             }

//             // Start the confirmation timer to commit the pending character after a delay.
//             esp_err_t err = esp_timer_start_once(confirmation_timer_handle, 1000000ULL);
//             if (err != ESP_OK)
//                 ESP_LOGE(TAG, "Failed to start confirmation timer: %s", esp_err_to_name(err));
//         }      

//         keypad.lastTick = currentTick;
//         int32_t event = EVT_KEYPAD_PRESS;
//         xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));

//         break;
//     }
// }

// // Timer callback to confirm the pending character if no other key is pressed
// void confirmation_timer_callback(void *arg)
// {
//     const char *TAG = "confirmation_timer_callback";
//     if (keypad.textBuffer == NULL || keypad.bufferSize == 0)
//         return;

//     // If a pending character exists (indicated by an active sequence), commit it.
//     if (keypad.lastPressedButton != -1)
//     {
//         if (keypad.bufferPos < keypad.bufferSize - 2)        
//             keypad.bufferPos++; // The pending character now becomes confirmed.        
//         else        
//             ESP_LOGW(TAG, "Buffer full on commit");
        
//         // Reset multi-tap state.
//         keypad.lastPressedButton = -1;
//         keypad.currentPressCount = 0;
//     }

//     int32_t event = EVT_KEYPAD_PRESS;
//     xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
// }




// // char* mainMenuEntries[] = {"Scan tag", "Transmit tag", "test", "test111"};
// menu_t* currentMenu = &mainMenu;

// void ui_handler_task(void* args)
// {
//     int32_t event;      
//     currentMenu->draw_func();    
//     for (;;)
//     {        
//         if (xQueueReceive(uiEventQueue, &event, pdMS_TO_TICKS(30000)) == pdTRUE)
//         {            
//             ESP_LOGI("ui_handler","Event: %ld\tMenu: %d", event, currentMenu->menuName);
//             // Allow back navigation from any state
//             if (event == KEY_BACK)
//             {
//                 if (currentMenu->back_handler_func != NULL)
//                 {
//                      menu_t *perviousMenu = currentMenu->back_handler_func();
//                     if (perviousMenu != NULL)                    
//                     {                       
//                         if (currentMenu->exit_func)
//                             currentMenu->exit_func();                        
//                         currentMenu = perviousMenu;
//                         if (currentMenu->enter_func)
//                             currentMenu->enter_func();                            
//                     }
//                 }
//                 else
//                 {
//                     menu_t *perviousMenu = stack_pop();
//                     if (perviousMenu != NULL)
//                     {
//                         if (currentMenu->exit_func)
//                             currentMenu->exit_func();

//                         currentMenu = perviousMenu;

//                         if (currentMenu->enter_func)
//                             currentMenu->enter_func();
//                     }
//                 }
//             }
//             else
//             {   // Let the current state handle the event
//                 if (currentMenu->event_handler_func)
//                 {
//                     menu_t *nextMenu = currentMenu->event_handler_func(event);

//                     // Check if a state transition is needed
//                     if (nextMenu != NULL)
//                     {
//                         // Push current state to stack for "back" functionality
//                         stack_push(currentMenu);

//                         // Perform exit action of old state
//                         if (currentMenu->exit_func)
//                             currentMenu->exit_func();

//                         // Officially change state
//                         currentMenu = nextMenu;

//                         // Perform entry action of new state
//                         if (currentMenu->enter_func)
//                             currentMenu->enter_func();
//                     }
//                 }
//             }

//             // Redraw the screen with the current (or new) state
//             if (currentMenu->draw_func) 
//                 currentMenu->draw_func();
            
//         }
//     }    
// }

// void display_delay_timer_callback()
// {
//     int32_t event = EVT_NEXT_MENU;
//     xQueueSendToBack(uiEventQueue, &event, pdMS_TO_TICKS(15));
// }

// void tag_tx_cycle_callback()
// {
//     int i = 0;
//     if (locNum <= 0) return;

//     for (;;)
//     {
//         uint64_t tag = rfid_array_to_tag(bestLocs[i].tag);
//         rfid_enable_tx_raw_tag(tag);
//         ESP_LOGI("tag_tx_cycle", "loc: %s\ttag: 0x%010llX", bestLocs[i].name, tag);
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//         i++;
//         if (i >= locNum)
//             i = 0;
//     }
// }