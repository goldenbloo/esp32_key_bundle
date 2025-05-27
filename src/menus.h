#ifndef MENUS_H
#define MENUS_H
#include "ssd1306.h"
#include "esp_timer.h"
#include "ssd1306.h"


extern esp_timer_handle_t confirmation_timer_handle;

void list_test(SSD1306_t* dev);
void display_wifi_aps(SSD1306_t* dev, wifi_ap_record_t *ap_records, uint16_t ap_count);
void display_loc_save(SSD1306_t* dev, QueueHandle_t actionQueue);
void keypad_button_press(int pressedButton, char *bufferPtr, uint8_t size, SSD1306_t *dev);
void confirmation_timer_callback(void *arg);
#endif