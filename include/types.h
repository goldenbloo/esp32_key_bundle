#ifndef TYPES_H
#define TYPES_H
#include "macros.h"

//--------------------Enums----------------------------------------------------
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

typedef enum
{
  KEY_TYPE_NONE,
  KEY_TYPE_RFID,
  KEY_TYPE_KT2,
  KEY_TYPE_DALLAS,

} key_type_enum;

//-------------------Structs---------------------------------------------------
// RFID------------------------------------------
typedef struct {
    bool level;
    uint32_t duration;
} rfid_input_event_t;

typedef struct
{
  bool isSynced;
  bool currentBit;
  bool lastBit;
  bool checkNextEdge; 
  bool bitIsReady;
  uint64_t tagInputBuff;  
} manchester_t;

// Touch-----------------------------------------
typedef struct {
    bool level;    
    uint32_t duration;
} touch_input_evt;

typedef struct
{
   uint32_t timeAvg;
   uint32_t tick;
   uint8_t evt;
   uint8_t bitCnt;
   uint32_t data;
   uint32_t duration;
} touch_print_t;

typedef struct {    
    uint32_t sumCnt;
    uint32_t timeAvg;
    uint32_t timeSum;
    uint32_t skipCnt;
    uint32_t timeLow;
    uint32_t timeHigh;
   
    uint8_t bitCnt;
    uint8_t startWordCnt;
    uint8_t startWord;
    uint8_t parity;

    bool timeAvgCalculated;
    bool syncBitFound;
    bool startOk;
} kt1233_decoder_t;

// Menus-----------------------------------------
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
    menu_t* menus[MENU_STACK_MAX_SIZE];
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

// LittleFS--------------------------------------
typedef union {
    uint64_t value;          
    uint8_t bytes[8];
    struct { uint64_t rom; } ds1990;
    struct { uint8_t  id[5]; } rfid;
    struct { uint32_t id; } kt2;
} key_data_t;

typedef struct __attribute__((packed))
{
  key_data_t keyData;
  int32_t id;  
  uint16_t keyType;
  uint8_t bssids[5][6];
  int8_t rssis[5];
  char name[FIELD_SIZE];
} location_t;


#endif // TYPES_H