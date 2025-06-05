#ifndef RFID_H
#define RFID_H
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "macros.h"

extern QueueHandle_t inputIsrEvtQueue;

typedef struct {
    bool level;
    uint32_t ms;  // Optional: store a ms if needed
    uint32_t idx;
    uint64_t buf;
    uint8_t tag[5];
} rfid_read_event_t;

typedef struct
{
  bool isSynced;
  bool currentBit;
  bool lastBit;
  bool checkNextEdge; 
  bool bitIsReady;
  uint64_t tagInputBuff;  
} manchester_t;

/*
 * checkNextEdge indicates that the *next* GPIO edge
 * corresponds to the trailing edge of a half-bit interval,
 * not to a new bit.  When true, you should:
 *   - NOT invert currentBit on that edge,
 *   - AND clear the flag afterward.
 */

char* int_to_char_bin(char* str, uint64_t num);
void rfid_read_isr_handler(void *arg);
uint64_t tagId_to_raw_tag(uint8_t *tagArr);
void raw_tag_to_rmt(rmt_symbol_word_t *rmtArr, uint64_t rawTag);
void enable_read_tag();
void enable_tx_tag(uint64_t tag);
void disable_rx_tx_tag();


typedef struct
{
  uint32_t id;
  char locName[30];
  uint8_t bssids[5][6];
  int8_t rssis[5];
} location_t;

extern rmt_channel_handle_t tx_chan;
extern rmt_encoder_handle_t copy_enc;
extern rmt_transmit_config_t trans_config;
extern rmt_symbol_word_t pulse_pattern[RMT_SIZE];

#endif