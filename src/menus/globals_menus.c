#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_wifi.h"
#include "macros.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rfid.h"
#include "menus.h"
#include "littlefs_records.h"


TaskHandle_t scrollTaskHandle;
scroll_data_t sd;

wifi_ap_record_t ap_records[BSSID_MAX];
uint16_t ap_count = 0;

location_t bestLocs[LOC_NUM_MAX];
uint32_t bestLocsNum;
char *locNameList[LOC_NUM_MAX + 1];
location_t chosenLoc;

menu_t* menuStack[MENU_STACK_SIZE];
int stack_top = -1;

char fieldBuffer[FIELD_SIZE];
keypad_t keypad = {
.textBuffer = fieldBuffer,
.bufferSize = sizeof(fieldBuffer),
.bufferPos = -1,
.lastPressedButton = -1,
.displayCharWidth= 16,
};