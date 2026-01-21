#ifndef TOUCH_H
#define TOUCH_H
#include "freertos/FreeRTOS.h"
#include "types.h"

extern QueueHandle_t touchInputIsrEvtQueue, printQueue;
extern rmt_channel_handle_t touch_tx_ch;
extern rmt_encoder_handle_t copy_enc;
extern rmt_transmit_config_t touch_tx_config;

void touch_rx_enable();
void touch_rx_disable();
void comp_rx_isr_handler(void *arg);
void touch_isr_deferred_task(void* args);
void transmit_metakom_k2();
void kt2_read_edge(uint8_t level, uint32_t duration, kt1233_decoder_t* decoder);
void touch_read_task(void* args);



#endif