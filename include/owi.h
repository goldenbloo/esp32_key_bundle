#ifndef OWI_H
#define OWI_H
#include "freertos/FreeRTOS.h"

extern QueueHandle_t touchInputIsrEvtQueue, printQueue;

void read_ds18b20();



#endif