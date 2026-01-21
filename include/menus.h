#ifndef MENUS_H
#define MENUS_H
#include "esp_timer.h"
#include "driver/rmt_tx.h"
#include "esp_wifi.h"
#include "littlefs_records.h"
#include "bitmaps.h"
#include "u8g2.h"
#include "types.h"

extern esp_timer_handle_t confirmation_timer_handle, display_delay_timer_handle;
extern ui_event_e display_delay_cb_arg;
extern QueueHandle_t uiEventQueue, modeSwitchQueue;
// extern SSD1306_t* devPtr;
extern SemaphoreHandle_t scanSem, scanDoneSem, rfidDoneSem, scrollDeleteSem, drawMutex;
extern key_data_t currentKeyData;
extern uint16_t currentKeyType;
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

