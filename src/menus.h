#ifndef MENUS_H
#define MENUS_H
#include "ssd1306.h"
#include "esp_timer.h"
#include "driver/rmt_tx.h"
#define MAX_SIZE 5

typedef enum
{
    POP_ERROR = -2,
    PUSH_ERROR = -1,
    MAIN_MENU,
    TAG_SCAN,
    WIFI_SCAN,
    DESCRIPTION_PROMPT,
    SAVE_TAG_PROMPT,
    DISCARD_PROMPT,
    OVERWRITE_OR_NEW_PROMPT,
    OVERWRITE,
    NEW,
    WIFI_SCAN_MATCH_SEARCH,
    NO_MATCH,
    ONE_MATCH,
    MULTIPLE_MATHES,

} menu_e;



typedef struct menu_t menu_t;
typedef struct menu_t 
{
    menu_e menuWindow;
    char** list;
    uint8_t listSize;
    int8_t selectedRow;
    int8_t topRowIdx;
    uint8_t maxPages;
    uint8_t startPage;

    int8_t selectedOption;

    uint8_t textFieldPage;

    void (*displayFunc) (menu_t*, char);

} menu_t;

typedef struct
{
    menu_t* menus[MAX_SIZE];
    int8_t top;
} menu_stack_t;

typedef enum
{    
    KEY_0,KEY_1,KEY_2,KEY_3,
    KEY_4,KEY_5,KEY_6,KEY_7,
    KEY_8,KEY_9,

    KEY_BACK,
    KEY_SHIFT,

    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,   
     
    EVT_RFID_SCAN_DONE,    
    EVT_WIFI_SCAN_DONE,
    EVT_NEXT_MENU,
} ui_event_e;

extern esp_timer_handle_t confirmation_timer_handle, display_delay_timer_handle;
extern QueueHandle_t uiEventQueue, modeSwitchQueue;
extern SSD1306_t* devPtr;
extern SemaphoreHandle_t scanSem, scanDoneSem, rfidDoneSem;
extern menu_t currentMenu;
extern uint64_t currentTag;
extern rmt_channel_handle_t tx_chan;
extern rmt_encoder_handle_t copy_enc;
extern rmt_transmit_config_t trans_config;
extern rmt_symbol_word_t pulse_pattern[RMT_SIZE];
extern TaskHandle_t uiHandlerTask;

void list_test();
void ssd1306_display_wifi_aps(wifi_ap_record_t *ap_records, uint16_t ap_count, uint32_t startPage);
void display_loc_save(QueueHandle_t keyEventQueue);
void keypad_button_press(int8_t pressedButton, char *bufferPtr, uint8_t size);
void confirmation_timer_callback(void *arg);
void ui_handler_task(void* args);
void display_delay_timer_callback();
#endif