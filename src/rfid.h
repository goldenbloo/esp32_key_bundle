#ifndef RFID_H
#define RFID_H
#include "freertos/FreeRTOS.h"

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
  bool checkNextEdge ;
  bool bitIsReady;
  uint64_t tagInputBuff;  
} manchester_t;

char* int_to_char_bin(char* str, uint64_t num);
void rfid_read_isr_handler(void *arg);
uint64_t tagId_to_raw_tag(uint8_t *tagArr)


#endif