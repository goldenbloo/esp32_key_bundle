#ifndef MENUS_H
#define MENUS_H
#include "esp_timer.h"
#include "driver/rmt_tx.h"
#include "esp_wifi.h"
#include "littlefs_records.h"
#include "bitmaps.h"
#include "u8g2.h"
#define MAX_SIZE 10

typedef enum
{
    MAIN_MENU,
    TAG_SCAN,
    WIFI_SCAN,
    DESCRIPTION_PROMPT,  
    REWRITE_MATCH_LOC_PROMPT, 
    SAVE_TAG_MENU,
    TRANSMIT_TAG_MENU,
    SEARCH_LOC_MENU,
    FOUND_LOC_LIST_MENU,
    LOC_EDIT_OPTIONS_MENU,
    LOC_NAME_EDIT_MENU,
    LOC_WIFI_APS_UPDATE_MENU,
    LOC_TAG_UPDATE_MENU,
    LOC_DELETE_MENU,
    LOC_TRANSMIT_TAG,
    

} menu_e;

typedef enum
{    
    EVT_INVALID = -1,
    EVT_NONE = 0,
    KEY_0,KEY_1,KEY_2,KEY_3,
    KEY_4,KEY_5,KEY_6,KEY_7,
    KEY_8,KEY_9,

    KEY_CLEAR_CHAR,
    KEY_SHIFT,

    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,   
    KEY_BACK,

    EVT_KEY_SCAN_DONE,    
    EVT_WIFI_SCAN_DONE,
    EVT_NEXT_MENU,
    EVT_KEYPAD_PRESS,
    EVT_ON_ENTRY,
    EVT_NO_MATCH,
    EVT_SEARCH_NOT_FOUND,
    EVT_SEARCH_FOUND,
    EVT_APS_NOT_FOUND,
    EVT_OVERWRITE_TAG,
    EVT_SAVE_SUCCESS,
    EVT_SAVE_FAIL,
    EVT_DELETE_SUCCESS,
    EVT_DELETE_FAIL,


} ui_event_e;

typedef enum
{
    NOT_SELECTED,
    YES_OPTION,
    NO_OPTION,
    SAVE_OPTION,
    OVERWRITE_OPTION,
    SAVE_NEW_OPTION,
    CANCEL_OPTION,
    DELETE_OPTION,
    
} option_e;

typedef struct
{
    char** list;
    uint8_t listSize;
    int8_t selectedRow;
    int8_t topRowIdx;
    int8_t maxRows;
} menu_listbox_t;

typedef struct
{
    // uint8_t textFieldPage;
    char* textFieldBuffer;
    uint8_t textFieldBufferSize;
} menu_textbox_t;

typedef struct menu_t menu_t;
typedef struct menu_t 
{
    uint16_t menuId;
    
    uint8_t startPosY;
    int8_t selectedOption;
    int32_t status;

    menu_listbox_t* listBox;  
    menu_textbox_t* textBox;      
    menu_t* nextMenu;
    
    menu_t* (*event_handler_func )(ui_event_e event);
    void    (*enter_func  )();
    void    (*exit_func   )();
    void    (*draw_func   )();
    menu_t* (*back_handler_func)();

} menu_t;

typedef struct
{
    menu_t* menus[MAX_SIZE];
    int8_t top;
} menu_stack_t;

typedef struct 
{
    char* textBuffer;
    int8_t bufferSize;
    int8_t bufferPos;
    char pendingChar; // Character currently being formed by multi-presses
    uint8_t letterIsCapital;
    int8_t lastPressedButton; // Index of the last button pressed 
    int8_t currentPressCount; // How many times the current button has been pressed in sequence
    int32_t lastTick; // Timestamp of the last valid button press 
    uint8_t displayCharWidth;
} keypad_t;


typedef struct 
{
    uint8_t textX, textY, bgBoxX, bgBoxY;
    bool invert;
    char* string;
    uint16_t strWidth;
    uint16_t delayScrollMs;
    uint16_t delayStartStopMs;    

}scroll_data_t;




extern esp_timer_handle_t confirmation_timer_handle, display_delay_timer_handle;
extern ui_event_e display_delay_cb_arg;
extern QueueHandle_t uiEventQueue, modeSwitchQueue;
// extern SSD1306_t* devPtr;
extern SemaphoreHandle_t scanSem, scanDoneSem, rfidDoneSem, scrollDeleteSem, drawMutex;
extern key_data_t currentKeyData;
extern rmt_channel_handle_t rfid_tx_ch;
extern rmt_encoder_handle_t copy_enc;
extern rmt_transmit_config_t rfid_tx_config;
extern rmt_symbol_word_t pulse_pattern[RMT_SIZE];
extern TaskHandle_t uiHandlerTask, rfidAutoTxHandler;

extern u8g2_t u8g2;


void list_test();
void display_wifi_aps(wifi_ap_record_t *ap_records, uint16_t ap_count, uint32_t startPage);
// void display_loc_save(QueueHandle_t keyEventQueue);
void keypad_button_press(ui_event_e pressedButton);
void confirmation_timer_callback(void *arg);
void ui_handler_task(void* args);
void display_delay_timer_callback();
void tag_tx_cycle_callback();
void display_list(menu_t *menu);
void scroll_text_task(void* arg);
menu_t* go_to_main_menu();
menu_t *go_to_loc_options_menu();
void list_event_handle(menu_t *menu, ui_event_e event);
void text_field_draw(uint32_t textFieldPosX, uint32_t textFieldPosY);
void scroll_task_stop();

void stack_push(menu_t* state);
menu_t* stack_pop();

//=======================================================================================
menu_t *main_menu_handle(ui_event_e event);
void main_menu_draw();
void main_menu_exit();

void scan_tag_menu_enter();
menu_t *scan_tag_menu_handle(ui_event_e event);
void scan_tag_menu_exit();
void scan_tag_menu_draw();

void scan_wifi_menu_enter();
menu_t *scan_wifi_menu_handle(ui_event_e event);
void scan_wifi_menu_exit();
void scan_wifi_menu_draw();

void save_tag_menu_enter();
menu_t *save_tag_menu_handle(ui_event_e event);
void save_tag_menu_draw();

void transmit_menu_enter();
menu_t *transmit_menu_handle(ui_event_e event);
void transmit_menu_exit();
void transmit_menu_draw();

void resolve_location_menu_enter();
menu_t* resolve_location_menu_handle(ui_event_e event);
void resolve_location_menu_exit();
void resolve_location_menu_draw();

void search_loc_menu_enter(); 
menu_t *search_loc_menu_handle(ui_event_e event);
void search_loc_menu_exit();
void search_loc_menu_draw();

void found_loc_list_menu_enter();
menu_t *found_loc_list_menu_handle(ui_event_e event);
void found_loc_list_menu_draw();

void loc_options_menu_enter();
menu_t *loc_options_menu_handle(ui_event_e event);
void loc_options_menu_draw();

void change_loc_name_menu_enter();
menu_t *change_loc_name_menu_handle(ui_event_e event);
void change_loc_name_menu_draw();

void update_wifi_menu_enter();
menu_t *update_wifi_menu_handle(ui_event_e event);
void update_wifi_menu_draw();

void update_tag_menu_enter();
menu_t* update_tag_menu_handle(ui_event_e event);
void update_tag_menu_draw();

void delete_loc_menu_enter();
menu_t* delete_loc_menu_handle(ui_event_e event);
void delete_loc_menu_draw();

void transmit_loc_tag_menu_enter();
menu_t* transmit_loc_tag_menu_handle(ui_event_e event);
void transmit_loc_tag_menu_exit();
void transmit_loc_tag_menu_draw();

//=======================================================================================
extern menu_t mainMenu;
extern menu_t scanTagMenu;
extern menu_t saveTagMenu;
extern menu_t scanWifiMenu;
extern menu_t resolveLocationMenu;
extern menu_t saveTagMenu;
extern menu_t transmitMenu;
extern menu_t searchLocMenu;
extern menu_t foundLocListMenu;
extern menu_t locOptionsMenu;
extern menu_t changeLocNameMenu;
extern menu_t updateWifiMenu;
extern menu_t updateTagMenu;
extern menu_t deleteLocMenu;
extern menu_t transmitLocTagMenu;



#endif

