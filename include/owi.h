#ifndef OWI_H
#define OWI_H
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "types.h"

extern QueueHandle_t touchInputIsrEvtQueue, printQueue;
extern rmt_channel_handle_t owi_tx_ch;
extern rmt_encoder_handle_t copy_enc;
extern rmt_transmit_config_t owi_rmt_tx_config;

uint64_t read_ds18b20();
bool owi_reset();
bool owi_read_rom(uint8_t *rom_buffer);
void owi_slave_enable();
void owi_emulation_isr(void* arg);
void owi_match_rom(uint64_t rom);
int8_t owi_search_rom(owi_rom_t* romArr, uint8_t arrSize);
void owi_print_scratchpad();

#endif