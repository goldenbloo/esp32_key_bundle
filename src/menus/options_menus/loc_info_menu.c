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
#include "globals_menus.h"
#define SCROLL_DELTA 8
#define SPACE_SIZE 1


menu_t locInfoMenu = {
    .menuId = LOC_INFO_MENU,    
    .startPosY = 8,
    .draw_func = loc_info_menu_draw,
    .event_handler_func = loc_info_menu_handle,
    .enter_func = loc_info_menu_enter,
    .back_handler_func = go_to_loc_options_menu,
   
};
static int32_t scrollOffset = 0;
static int32_t scrollMax = 0;


void loc_info_menu_enter()
{   
    locInfoMenu.status = EVT_ON_ENTRY;
    scrollOffset = 0;
    int16_t totalHeight = 0;

    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);
    uint8_t fontHeight = u8g2_GetMaxCharHeight(&u8g2);
    // 5 labels + 4 data strings (ID, Name, Type, Value, APs)
    totalHeight = (9 * (fontHeight + SPACE_SIZE)); 

    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);
    fontHeight = u8g2_GetMaxCharHeight(&u8g2);
    for (int i = 0; i < BSSID_MAX; i++) 
    {
        bool bssidtEmpty = true;
        for (uint8_t j = 0; j < 6; j++)
        {
            if (currentLoc.bssids[i][j] != 0)
            {
                bssidtEmpty = false;
                break;
            }
        }
        if (!bssidtEmpty) 
            totalHeight += (fontHeight + SPACE_SIZE);
    }
    scrollMax = totalHeight - 64; 
    if (scrollMax < 0) scrollMax = 0;
    
}

menu_t* loc_info_menu_handle(ui_event_e event) {
    if (locInfoMenu.status == EVT_ON_ENTRY) locInfoMenu.status = EVT_NONE;

    switch (event) {
    case KEY_UP:
        scrollOffset -= SCROLL_DELTA;
        if (scrollOffset < 0) scrollOffset = 0;
        break;
    case KEY_DOWN:
        scrollOffset += SCROLL_DELTA;
        // Clamp to scrollMax (updated in draw function)
        if (scrollOffset > scrollMax) scrollOffset = scrollMax;
        break;   
    default:
        break;
    }
    return NULL;
}

void loc_info_menu_draw()
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_6x13_tr);
    int16_t displayHeight = u8g2_GetDisplayHeight(&u8g2);
    int16_t displayWidth = u8g2_GetDisplayWidth(&u8g2);
    uint8_t fontHeight = u8g2_GetMaxCharHeight(&u8g2);
    uint16_t posY = locInfoMenu.startPosY - scrollOffset + SPACE_SIZE;
    char str[30];

    #define DRAW_IF_VISIBLE(text)                                \
    if (posY > -fontHeight && posY < displayHeight + fontHeight) \
    {                                                            \
        u8g2_DrawUTF8(&u8g2, 0, posY, text);                     \
    }                                                            \
    posY += fontHeight + SPACE_SIZE;

    #define LINE_IF_VISIBLE()                                            \
    if (posY > 0 && posY < displayHeight)                                \
    {                                                                    \
        u8g2_DrawHLine(&u8g2, 0, posY - (SPACE_SIZE), displayWidth); \
    }

    // 1. Entry ID
    DRAW_IF_VISIBLE("Entry ID:");
    snprintf(str, sizeof(str), "%ld", currentLoc.id);
    DRAW_IF_VISIBLE(str);
    LINE_IF_VISIBLE()
    // 2. Name
    DRAW_IF_VISIBLE("Name:");
    DRAW_IF_VISIBLE(currentLoc.name);
    LINE_IF_VISIBLE()
    // 3. Key Type
    DRAW_IF_VISIBLE("Key Type:");
    DRAW_IF_VISIBLE(get_key_type_string(currentLoc.keyType));
    LINE_IF_VISIBLE()
    // 4. Key Value
    DRAW_IF_VISIBLE("Key Value:");    
    char* formatStr;
    switch (currentLoc.keyType){
        case KEY_TYPE_EM4100_MANCHESTER_64:            
            formatStr = "0x%010llX";
            break;
        case KEY_TYPE_KT2:
             formatStr = "0x%08llX";
            break;
        case KEY_TYPE_IBUTTON:            
            formatStr = "0x%016llX";
            break;
        default:
            formatStr = "0x%010llX";
         break;
        };
    snprintf(str, sizeof(str), formatStr, currentLoc.keyData.value);
    DRAW_IF_VISIBLE(str);
    LINE_IF_VISIBLE()
    // 5. WiFi List
    DRAW_IF_VISIBLE("WiFi list:");
    u8g2_SetFont(&u8g2, u8g2_font_NokiaSmallBold_tr);
    fontHeight = u8g2_GetMaxCharHeight(&u8g2);    
    for (uint8_t i = 0; i < BSSID_MAX; i++)
    {
        bool bssidtEmpty = true;
        for (uint8_t j = 0; j < 6; j++)
        {
            if (currentLoc.bssids[i][j] != 0)
            {
                bssidtEmpty = false;
                break;
            }
        }      
        if (!bssidtEmpty) 
        {
            snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X %d", 
                     currentLoc.bssids[i][0], currentLoc.bssids[i][1], currentLoc.bssids[i][2],
                     currentLoc.bssids[i][3], currentLoc.bssids[i][4], currentLoc.bssids[i][5], currentLoc.rssis[i]);
            DRAW_IF_VISIBLE(str);
        }  
        else if (bssidtEmpty && (i == BSSID_MAX - 1))
        {
            DRAW_IF_VISIBLE("NULL");  
        }  
              
        
       
    }

}
