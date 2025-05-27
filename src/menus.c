#include "freertos/FreeRTOS.h"
#include "ssd1306.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"

void list_test(SSD1306_t* dev)
{
    // Test display scrolling on button
        const char* list[] = {"zero","first","second","third","forth","fifth","sixth","seventh","eigth" ,"nineth", "tenth", "eleventh","EEEEOOOO"};        
        const uint8_t disp_rows = 7, list_size= sizeof(list)/sizeof(&list);  
        uint8_t str_size[list_size];
        for (int i = 0; i < list_size; i++)
        {
            str_size[i] = strlen(list[i]);            
        }

        static int8_t pos = 0, frame_top_pos = 0;
        pos++;
        if (pos > list_size - 1)
            pos = 0;
        if (pos > frame_top_pos + disp_rows)
        {
            frame_top_pos = pos - disp_rows;
            ssd1306_clear_screen(dev, false);
        }
        else if (pos < frame_top_pos)
        {
            frame_top_pos = pos;
            ssd1306_clear_screen(dev, false);
        }

        // ssd1306_clear_screen(&dev, false);
        for (int i = 0; i <= disp_rows; i++)
        {            
            ssd1306_display_text(dev, i, list[i+frame_top_pos], str_size[i+frame_top_pos], (i+frame_top_pos) == pos ? 1 : 0);
        }
}

void display_wifi_aps(SSD1306_t* dev, wifi_ap_record_t *ap_records, uint16_t ap_count)
{
    char str[20]; 
    ssd1306_clear_screen(dev, false);
    for (uint8_t i = 0; i < 5 && i < ap_count; i++)
    {
        sprintf(str,"%02x:%02x:%02x:%02x %d",  ap_records[i].bssid[2],            
                                                ap_records[i].bssid[3],
                                                ap_records[i].bssid[4],
                                                ap_records[i].bssid[5],
                                                ap_records[i].rssi);
        ssd1306_display_text(dev, i, str, strlen(str), false);
        
    }
    
}

// Display location save prompt
void display_loc_save(SSD1306_t* dev, QueueHandle_t actionQueue)
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
    ssd1306_clear_screen(dev, false);
    ssd1306_display_text(dev, 0, row0,sizeof(row0)-1, false);
    ssd1306_display_text(dev, 1, row1,sizeof(row1)-1, false);
    ssd1306_display_text(dev, 2, locName,sizeof(locName)-1, false);
    ssd1306_bitmaps(dev, 0, 55, yes_no, 128, 8, false);
    for (;;)
    {
        if (xQueueReceive(actionQueue, &action, pdMS_TO_TICKS(30000)) == pdTRUE)
        {
            switch (action)
            {
            case ACTION_UP:
                ssd1306_display_text(dev, 2, locName, sizeof(locName) - 1, true);
                ssd1306_bitmaps(dev, 0, 55, yes_no, 128, 8, false);
                break;
            case ACTION_DOWN:
            case ACTION_LEFT:
                ssd1306_display_text(dev, 2, locName, sizeof(locName) - 1, false);
                ssd1306_bitmaps(dev, 0, 55, YES_no, 128, 8, false);
                break;

            case ACTION_RIGHT:
                ssd1306_display_text(dev, 2, locName, sizeof(locName) - 1, false);
                ssd1306_bitmaps(dev, 0, 55, YES_no, 128, 8, true);
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
    {'0', ' '},               // Key '0'
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
esp_timer_handle_t confirmation_timer_handle;
static const char* TAG = "keypad";

void keypad_button_press(int pressedButton, char *bufferPtr, uint8_t size, SSD1306_t *dev)
{
    char temp[20] = {0};
    textBuffer = bufferPtr;
    bufferSize = size;
    // ESP_LOGD(TAG, "Button %d (GPIO %d) processing", pressedButton + 1, button_gpios[pressedButton]);

    // Stop any pending confirmation timer, as a new action is happening
    esp_timer_stop(confirmation_timer_handle);
    int32_t currentTick = esp_cpu_get_cycle_count();

    // switch (pressedButton)
    // {
    // case constant expression:
    //     /* code */
    //     break;
    
    // default:
    //     break;
    // }
    if (pressedButton == 10 )
    {
        if (bufferPos > 0)
        {
            textBuffer[bufferPos] = '\0';
            bufferPos--;
            textBuffer[bufferPos] = '\0';
            pendingChar = '\0';
        }
    }
    else if (lastPressedButton == pressedButton &&
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

                textBuffer[bufferPos] = pendingChar;
                bufferPos++;
                textBuffer[bufferPos] = '\0'; // Null-terminate
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
    lastTick = currentTick;

    sprintf(temp, "pc=%c bp=%d pb=%d", pendingChar, bufferPos, pressedButton);
    ssd1306_display_text(dev, 0, temp, 16, false);
    sprintf(temp, "PrCnt=%d lpb=%d", currentPressCount, lastPressedButton);
    ssd1306_display_text(dev, 1, temp, 16, false);
    ssd1306_display_text(dev, 2, textBuffer, 16, false);

    if (pressedButton != 10)
    {
        esp_err_t err = esp_timer_start_once(confirmation_timer_handle, 1000 * 1000ULL);
        if (err != ESP_OK)
        {
            // ESP_LOGE(TAG, "Failed to start confirmation timer: %s", esp_err_to_name(err));
        }
    }
}

// Timer callback to confirm the pending character if no other key is pressed
void confirmation_timer_callback(void *arg)
{
    SSD1306_t* dev = (SSD1306_t*)arg;
    if (textBuffer == NULL || bufferSize == 0) 
        return;

    if (pendingChar != '\0' && lastPressedButton != -1)
    {
        if (bufferPos < bufferSize - 1)
        {
            if (pendingChar > 0x60 && pendingChar < 0x7a && letterIsCapital)
                pendingChar -= 0x20; // Set as capital letter

            textBuffer[bufferPos] = pendingChar;
            bufferPos++;
            textBuffer[bufferPos] = '\0'; // Null-terminate
        }
        // ESP_LOGI(TAG, "Confirmed by timer: '%c'. Buffer: \"%s\"", pendingChar, textBuffer);

        pendingChar = '\0';
        lastPressedButton = -1; // Reset for next new key sequence
        currentPressCount = 0;
    }
    ssd1306_display_text(dev, 2, textBuffer, 16, false);
}