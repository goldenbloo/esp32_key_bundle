#ifndef TOUCH_H
#define TOUCH_H
#include "freertos/FreeRTOS.h"

extern QueueHandle_t touchInputIsrEvtQueue, printQueue;

typedef struct {
    bool level;    
    uint32_t currentTime;
} touch_input_evt;

typedef struct
{
   uint32_t timeAvg;
   uint32_t tick;
   uint8_t evt;
   uint8_t bitCnt;
   uint32_t data;
   uint32_t duration;
} touch_print_t;


void read_metakom_kt2();
void  comp_rx_isr_handler(void *arg);
void touch_memory_deferred_task(void* args);



#endif