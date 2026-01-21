#ifndef OWI_H
#define OWI_H
#include "freertos/FreeRTOS.h"

extern QueueHandle_t touchInputIsrEvtQueue, printQueue;

uint64_t read_ds18b20();
bool owi_reset();
bool owi_read_rom(uint8_t *rom_buffer);



#endif