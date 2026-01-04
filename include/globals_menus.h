#ifndef GLOBAL_MENUS
#define GLOBAL_MENUS


#include "macros.h"
#include "menus.h"

extern TaskHandle_t scrollTaskHandle;
extern scroll_data_t sd;

extern wifi_ap_record_t ap_records[BSSID_MAX];
extern uint16_t ap_count;

extern location_t bestLocs[LOC_NUM_MAX];
extern location_t currentLoc;
extern uint32_t bestLocsNum;
extern char *locNameList[LOC_NUM_MAX + 1];

extern menu_t* menuStack[MENU_STACK_SIZE];
extern int stack_top;

extern char fieldBuffer[FIELD_SIZE];
extern char searchMenuBuffer[FIELD_SIZE];
extern keypad_t keypad;
#endif