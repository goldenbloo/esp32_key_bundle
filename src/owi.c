#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "macros.h"
#include "owi.h"

portMUX_TYPE owi_mux = portMUX_INITIALIZER_UNLOCKED;

bool owi_reset()
{    
    gpio_set_level(OWI_TX, 1); // Open transistor, set line low    
    esp_rom_delay_us(485); // Reset pulse
    gpio_set_level(OWI_TX, 0); // Close transistor
    esp_rom_delay_us(70); // Wait for presence
    bool level = gpio_get_level(COMP_RX); // Read presence, signal reversed due to comparator
    esp_rom_delay_us(410);      // Wait for the rest of the slot
    return level;
}

void owi_write_bit(uint8_t bit)
{
    if ((bit & 0x1) == 0) // Write 0
    {
        gpio_set_level(OWI_TX, 1); // Transistor ON -> Line LOW
        esp_rom_delay_us(60);      // Hold LOW for 60us
        gpio_set_level(OWI_TX, 0); // Release
        esp_rom_delay_us(1);       // Recovery time
    }
    else // Write 1
    {
        gpio_set_level(OWI_TX, 1); // Transistor ON -> Line LOW
        esp_rom_delay_us(10);       // Short pulse 
        gpio_set_level(OWI_TX, 0); // Release
        esp_rom_delay_us(60);      // Wait for the rest of the slot
    }    
}

void owi_write_byte(uint8_t data)
{
    // taskENTER_CRITICAL(&owi_mux);
    for (uint8_t i = 0; i < 8; i++)
        owi_write_bit((data >> i) & 0x1);
    // taskEXIT_CRITICAL(&owi_mux);
}

bool owi_read_bit()
{
    bool level = 1;  
    gpio_set_level(OWI_TX, 1); // Transistor ON -> Line LOW
    esp_rom_delay_us(2);       // Hold LOW 
    gpio_set_level(OWI_TX, 0); // Release
    esp_rom_delay_us(10); 
    level = !gpio_get_level(COMP_RX); // Read level, signal reversed due to comparator
    esp_rom_delay_us(50); // Wait for the rest of the slot
    return level;
}

uint8_t owi_read_byte()
{
    // taskENTER_CRITICAL(&owi_mux);
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++)
        if (owi_read_bit())
            data |= (1 << i);
    // taskEXIT_CRITICAL(&owi_mux);
    return data;
}

// State variables for the search
static uint8_t currentROM[8];
static int last_discrepancy;
static int last_family_discrepancy;
static bool last_device_flag;

void owi_reset_search() {
    last_discrepancy = 0;
    last_device_flag = false;
    last_family_discrepancy = 0;
    for (int i = 0; i < 8; i++) currentROM[i] = 0;
}

bool owi_search(uint8_t *newAddr)
{
    int id_bit_number = 1;
    int last_zero = 0;
    int rom_byte_number = 0;
    uint8_t rom_byte_mask = 1;
    bool search_result = false;
    uint8_t id_bit, cmp_id_bit, search_direction;

    if (!last_device_flag)
    {
        if (!owi_reset())
        { // Your reset function
            owi_reset_search();
            return false;
        }

        owi_write_byte(0xF0); // Search ROM command
        do
        {   // 1. Read the bit and its complement
            id_bit = owi_read_bit();
            cmp_id_bit = owi_read_bit();

            if (id_bit == 1 && cmp_id_bit == 1) // No devices responded
                break;            
            else
            {
                if (id_bit != cmp_id_bit)
                {   // All devices have the same bit value at this position
                    search_direction = id_bit;
                }
                else
                {   // Discrepancy: some have 0, some have 1
                    if (id_bit_number < last_discrepancy)
                        search_direction = ((currentROM[rom_byte_number] & rom_byte_mask) > 0);
                    else
                        search_direction = (id_bit_number == last_discrepancy);

                    if (search_direction == 0)
                        last_zero = id_bit_number;
                    if (last_zero < 9)
                        last_family_discrepancy = last_zero;
                }
                // 2. Write the search direction to tell devices which one to stay active
                if (search_direction == 1)
                    currentROM[rom_byte_number] |= rom_byte_mask;
                else
                    currentROM[rom_byte_number] &= ~rom_byte_mask;

                owi_write_bit(search_direction);

                id_bit_number++;
                rom_byte_mask <<= 1;
                if (rom_byte_mask == 0)
                {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8);

        if (!(id_bit_number < 65))
        {
            last_discrepancy = last_zero;
            if (last_discrepancy == 0)
                last_device_flag = true;
            search_result = true;
        }
    }

    if (!search_result || !currentROM[0])
    {
        owi_reset_search();
        search_result = false;
    }
    else
        for (int i = 0; i < 8; i++)
            newAddr[i] = currentROM[i];

    return search_result;
}

uint8_t owi_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix)            
                crc ^= 0x8C; // Inverted polynomial for LSB-first            
            inbyte >>= 1;
        }
    }
    return crc;
}

void read_ds18b20()
{
    uint8_t rom[8];
    uint64_t rom64 = 0;
    uint8_t scratchpad[9];
    if (owi_reset())
        printf("DS18B20 present\n");
    else
        printf("DS18B20 not present\n");
    
    if (owi_search(rom))
    {
        for (uint8_t i = 0; i < 8; i++)
            rom64 |= rom[i] << (8 * i);
        printf("Found DS18B20 ROM: %llX\n", rom64);
        owi_reset();
        owi_write_byte(0x55); // Match ROM
        for (int i = 0; i < 8; i++) 
            owi_write_byte(rom[i]);
        owi_write_byte(0x44); // Convert T command
        vTaskDelay(pdMS_TO_TICKS(750));

        owi_reset();
        owi_write_byte(0x55); // Match ROM
        for (int i = 0; i < 8; i++) 
            owi_write_byte(rom[i]);
        owi_write_byte(0xBE);

        for (int i = 0; i < 9; i++) 
            scratchpad[i] = owi_read_byte();
        if (owi_crc8(scratchpad, 8) != scratchpad[8])
        {
            printf("Scratchpad CRC failed\n");
            return;
        }
        int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
        printf("Temp: %.2f°C\n", raw/16.0);
    }  
    else
    {
        printf("DS18B20 not found!\n");
    }  

}



