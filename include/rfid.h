#ifndef RFID_H
#define RFID_H
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "macros.h"
#include "types.h"

extern QueueHandle_t rfidInputIsrEvtQueue;



/*
 * checkNextEdge indicates that the *next* GPIO edge
 * corresponds to the trailing edge of a half-bit interval,
 * not to a new bit.  When true, you should:
 *   - NOT invert currentBit on that edge,
 *   - AND clear the flag afterward.
 */
void syncErrorFunc(manchester_t *m);
void manchester_read(uint8_t level, uint32_t duration);
char* int64_to_char_bin(char* str, uint64_t num);
char *int32_to_char_bin(char *str, uint32_t num);
void rfid_read_isr_handler(void *arg);
uint64_t rfid_arr_tag_to_raw_bitstream(uint8_t *tagArr);
// void rfid_array_to_rmt(rmt_symbol_word_t *rmtArr, uint64_t rawTag);
void rfid_enable_rx_tag();
void rfid_enable_tx_raw_tag(uint64_t tag);
void rfid_disable_rx_tx_tag();
void rfid_int_to_array(uint64_t tag, uint8_t tagArr[]);
uint64_t rfid_array_to_tag(uint8_t tagArr[]);

void rfid_deferred_task(void *arg);

extern rmt_channel_handle_t rfid_tx_ch;
extern rmt_encoder_handle_t copy_enc;
extern rmt_transmit_config_t rfid_tx_config;
extern rmt_symbol_word_t pulse_pattern[RMT_SIZE];
extern QueueHandle_t uiEventQueue;

#endif