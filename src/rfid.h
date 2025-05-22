#ifndef RFID_H
#define RFID_H
#include "freertos/FreeRTOS.h"
#include <hal/rmt_types.h>

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

typedef struct
{
  uint32_t id;
  char locName[30];
  uint8_t bssids[5][6];
  int8_t rssis[5];
} location_t;

#endif