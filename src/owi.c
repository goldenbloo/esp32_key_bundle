#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "string.h"
#include "macros.h"
#include "owi.h"
#include "touch.h"


static volatile uint32_t lastIsrTime = 0;
static volatile owi_device_t dev = {
    .rom = 0xf6021830f375ff28,
};

bool owi_reset()
{    
    gpio_set_level(OWI_TX_2, 1); // Open transistor, set line low    
    esp_rom_delay_us(485); // Reset pulse
    gpio_set_level(OWI_TX_2, 0); // Close transistor
    esp_rom_delay_us(70); // Wait for presence
    bool level = gpio_get_level(COMP_RX); // Read presence, signal reversed due to comparator
    esp_rom_delay_us(410);      // Wait for the rest of the slot
    // static print_t evt;
    // evt.evt = 9; evt.level = level;
    // xQueueSendToBackFromISR(printQueue, &evt, NULL);    

    return !level;
}

void owi_write_bit(uint8_t bit)
{
    if ((bit & 0x1) == 0) // Write 0
    {
        gpio_set_level(OWI_TX_2, 1); // Transistor ON -> Line LOW
        esp_rom_delay_us(60);      // Hold LOW for 60us
        gpio_set_level(OWI_TX_2, 0); // Release
        esp_rom_delay_us(10);       // Recovery time
    }
    else // Write 1
    {
        gpio_set_level(OWI_TX_2, 1); // Transistor ON -> Line LOW
        esp_rom_delay_us(7);       // Short pulse 
        gpio_set_level(OWI_TX_2, 0); // Release
        esp_rom_delay_us(63);      // Wait for the rest of the slot
    }    
}

void owi_write_byte(uint8_t data)
{  
    for (uint8_t i = 0; i < 8; i++)
        owi_write_bit((data >> i) & 0x1);
}

bool owi_read_bit()
{
    bool level;  
    gpio_set_level(OWI_TX_2, 1); // Transistor ON -> Line LOW
    esp_rom_delay_us(6);       // Hold LOW 
    gpio_set_level(OWI_TX_2, 0); // Release
    esp_rom_delay_us(10); 
    level = gpio_get_level(COMP_RX); // Read level, signal reversed due to comparator
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

bool owi_read_rom(uint8_t *rom_buffer) 
{
    if (!owi_reset()) 
        return false; // No key present    

    owi_write_byte(0x33); // Read ROM command
    for (int i = 0; i < 8; i++) 
        rom_buffer[i] = owi_read_byte();
    
    // Verify CRC
    if (owi_crc8(rom_buffer, 7) != rom_buffer[7]) 
        return false; // Checksum failed
    return true; 
}

int8_t owi_search_rom(owi_rom_t* pRom, uint8_t arrSize)
{
    uint8_t LastDiscrepancy = 0;
    uint8_t LastDeviceFlag = 0;
    uint8_t device_found= 0;
    uint8_t romIndex = 0;
    owi_rom_t currentRom = {0};
    do
    {       
        uint8_t id_bit_number = 1;
        uint8_t last_zero = 0, rom_byte_number = 0;
        uint8_t id_bit, cmp_id_bit;
        uint8_t rom_byte_mask = 1, search_direction;

        if (!LastDeviceFlag)
        {
            // 1-Wire reset
            if (!owi_reset()) return 0;
                   
            owi_write_byte(0xF0);
            // loop to do the search
            do
            {
                // read a bit and its complement
                id_bit = owi_read_bit();
                cmp_id_bit = owi_read_bit();

                // check for no devices on 1-wire
                if ((id_bit == 1) && (cmp_id_bit == 1))
                    break;
                else
                {                   
                    if (id_bit != cmp_id_bit)
                        search_direction = id_bit; 
                    else
                    {
                        if (id_bit_number < LastDiscrepancy)
                            search_direction = ((currentRom.idArr[rom_byte_number] & rom_byte_mask) > 0);
                        else                            
                            search_direction = (id_bit_number == LastDiscrepancy);

                        // if 0 was picked then record its position in LastZero
                        if (search_direction == 0)
                        {
                            last_zero = id_bit_number;
                        }
                    }
                    if (search_direction == 1)
                        currentRom.idArr[rom_byte_number] |= rom_byte_mask;
                    else
                        currentRom.idArr[rom_byte_number] &= ~rom_byte_mask;

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

            if (rom_byte_number == 8)
            { // We found a full 64-bit ROM
                if (owi_crc8(currentRom.idArr, 8) == 0)
                {
                    // SUCCESS: Check bounds before storing
                    if (romIndex < arrSize)
                    {
                        pRom[romIndex].id64 = currentRom.id64;
                        romIndex++;
                    }
                    else                    
                        return -1; // Buffer full                    

                    device_found = 1;
                    LastDiscrepancy = last_zero;
                    if (LastDiscrepancy == 0)
                        LastDeviceFlag = 1;
                }
                else
                {
                    // CRC Fail: Stop to prevent infinite loops on bad data
                    return -2;
                }
            }
        }

        // if no device found then reset counters so next 'search' will be like a first
        if (!device_found || currentRom.id64 == 0)
        {
            LastDiscrepancy = 0;
            LastDeviceFlag = false;
            device_found = false;
        }
//    } while (!LastDeviceFlag);
   } while (0);


   return romIndex;
}

uint64_t read_ds18b20()
{
    owi_rom_t rom[8];
    uint64_t rom64 = -1;
    uint8_t scratchpad[9];
    if (owi_reset())
        printf("DS18B20 present\n");
    else
        printf("DS18B20 not present\n");

    int8_t romCnt = owi_search_rom(rom,sizeof(rom)/sizeof(owi_rom_t));
    
    if (romCnt > 0)
    {
        for (uint8_t romIdx = 0; romIdx < romCnt; romIdx++)
        {            
            printf("Found DS18B20 ROM: %llX\n", rom[romIdx].id64);
            owi_reset();
            owi_write_byte(0x55); // Match ROM
            for (int i = 0; i < 8; i++)
                owi_write_byte(rom[romIdx].idArr[i]);
            owi_write_byte(0x44); // Convert T command
            vTaskDelay(pdMS_TO_TICKS(750));

            owi_reset();
            owi_write_byte(0x55); // Match ROM
            for (int i = 0; i < 8; i++)
                owi_write_byte(rom[romIdx].idArr[i]);
            owi_write_byte(0xBE);

            for (int i = 0; i < 9; i++)
                scratchpad[i] = owi_read_byte();
            if (owi_crc8(scratchpad, 8) != scratchpad[8])
            {
                printf("Scratchpad CRC failed\n");
                return rom64;
            }
            int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
            printf("Temp: %.2f°C\n", raw / 16.0);
        }
    }
    else
    {
        printf("DS18B20 not found!\n");
    }  
    return rom64;

}

void owi_print_scratchpad()
{
    uint8_t scratchpad[9];

    if (!owi_reset()) 
        return ; // No key present

    owi_write_byte(0xcc);
    owi_write_byte(0x44);
    vTaskDelay(pdMS_TO_TICKS(750));
    owi_reset();
    owi_write_byte(0xcc);
    owi_write_byte(0xBE);
    // 2. Read all 9 bytes of the scratchpad
    printf("Scratchpad Data: ");
    for (int i = 0; i < 9; i++)
    {
        scratchpad[i] = owi_read_byte();
        printf("%02X ", scratchpad[i]);
    }
    printf("\n");
    // 3. Optional: Extract temperature (for DS18B20)
    int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
    float temperature = raw_temp / 16.0;
    printf("Temperature: %.2f°C\n", temperature);
}

void owi_match_rom(uint64_t rom)
{
    if (!owi_reset()) 
        return;
    owi_write_byte(0x55);
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t byte = (rom >> (i * 8)) & 0xFF;
        owi_write_byte(byte);
    }
}

void owi_slave_enable()
{
    dev.state = OWI_STATE_IDLE;
    dev.bitCnt = 0;
    dev.byteIdx = 0;
    dev.currentByte = 0;
    gpio_set_level(PULLUP_PIN, 0); // for testing

    // ESP_ERROR_CHECK(gpio_intr_disable(COMP_RX));
    // ESP_ERROR_CHECK(gpio_isr_handler_remove(COMP_RX));
    // ESP_ERROR_CHECK(gpio_isr_handler_add(COMP_RX, owi_emulation_isr, NULL));

    // ESP_ERROR_CHECK(gpio_set_intr_type(COMP_RX, GPIO_INTR_ANYEDGE));
    // ESP_ERROR_CHECK(gpio_intr_enable(COMP_RX));
}

static void device_reset()
{        
    dev.state = OWI_STATE_IDLE;
    dev.edgeIdx = 0;
    dev.currentByte = 0;
    dev.bitCnt = 0;
    dev.byteIdx = 0;
    dev.txBuffer = NULL;
    dev.txLen = 0;
    dev.searchROMstate = 0;
    // dev.rom = 0xf6021830f375ff28; //test
}

static inline void owi_slave_presence()
{
    GPIO.out1_w1ts.val = (1UL << (OWI_TX - 32)); // Pull low
    esp_rom_delay_us(100);
    GPIO.out1_w1tc.val = (1UL << (OWI_TX - 32)); // Release high
    esp_rom_delay_us(2);
    GPIO.status1_w1tc.val = (1UL << (OWI_TX - 32));
    lastIsrTime = esp_cpu_get_cycle_count();
}

static inline void owi_slave_write_zero()
{
    GPIO.out1_w1ts.val = (1UL << (OWI_TX - 32)); // Pull low
    esp_rom_delay_us(55);
    GPIO.out1_w1tc.val = (1UL << (OWI_TX - 32)); // Release high
    esp_rom_delay_us(2);
    GPIO.status1_w1tc.val = (1UL << (OWI_TX - 32));
    lastIsrTime = esp_cpu_get_cycle_count();
}

void IRAM_ATTR owi_emulation_isr(void *arg)
{    
    static print_t evt;
    // uint32_t status = GPIO.status;
    // GPIO.status_w1tc = (1ULL << COMP_RX);    
    static volatile uint8_t level = true;
    uint32_t now = esp_cpu_get_cycle_count();
    static uint32_t resetTime;
    uint32_t duration_ticks = now - lastIsrTime;
    lastIsrTime = now;
    uint32_t duration_us = duration_ticks / CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    dev.edgeIdx++;

    // if (duration_us > 550)
    //     device_reset();

    level = (GPIO.in1.val >> (COMP_RX - 32)) & 1;
    // if (dev.state == OWI_STATE_IDLE)
    //     level = (GPIO.in1.val >> (COMP_RX - 32)) & 1;
    // else
    //     level = !level;    
    // if (level) GPIO.out_w1ts = (1UL << LED_PIN);
    // else       GPIO.out_w1tc = (1UL << LED_PIN);
    // evt.evt = 1; evt.duration = (now - resetTime) / 160;  evt.level = !level; evt.cnt = dev.edgeIdx;
    // xQueueSendToBackFromISR(printQueue, &evt, NULL);

    if (level && duration_us > 460 && duration_us < 600)
    {
        device_reset();
        resetTime = esp_cpu_get_cycle_count();
        
        evt.evt = 3;
        xQueueSendToBackFromISR(printQueue, &evt, NULL);
        // rmt_transmit(owi_tx_ch, copy_enc, &presence_symbol, sizeof(presence_symbol), &owi_rmt_tx_config);
        owi_slave_presence();
        
        dev.state = OWI_STATE_PRESENCE_SENDING;

        return;
    }

    switch (dev.state)
    {
    case OWI_STATE_PRESENCE_SENDING:      
        
            dev.state = OWI_STATE_RECEIVING_COMMAND;
            evt.evt = 4;
            xQueueSendToBackFromISR(printQueue, &evt, NULL);        
        break;

    case OWI_STATE_RECEIVING_COMMAND:
        // Sample on the Rising Edge (when master releases the line)
        if (level == 1)
        {
            // If duration was short, it's a 1. If long, it's a 0.
            uint8_t bit = (duration_us < 35) ? 1 : 0;
            dev.currentByte |= (bit << dev.bitCnt);

            // evt.evt = 6; evt.level = bit; evt.cnt = dev.bitCnt;
            // xQueueSendToBackFromISR(printQueue, &evt, NULL);

            dev.bitCnt++;
            if (dev.bitCnt == 8)
            {
                evt.evt = 2;
                evt.level = dev.currentByte;
                xQueueSendToBackFromISR(printQueue, &evt, NULL);

                switch (dev.currentByte)
                {
                case 0x33: // Read ROM
                    dev.state = OWI_STATE_TRANSMITTING_DATA;
                    dev.txBuffer = (uint8_t *)&dev.rom;
                    dev.txLen = sizeof(dev.rom);
                    // evt.evt = 7;
                    // xQueueSendToBackFromISR(printQueue, &evt, NULL);
                    break;

                case 0xcc: // Skip ROM
                    dev.state = OWI_STATE_RECEIVING_COMMAND;
                    break;

                case 0x55: // Match ROM
                    dev.state = OWI_STATE_RECEIVE_ROM;
                    break;

                case 0xF0: // Search ROM
                    dev.state = OWI_STATE_SEARCH_ROM;
                    dev.searchROMstate = 0;
                    break;

                default:
                    device_reset();
                    break;
                }
                dev.bitCnt = 0;
                dev.byteIdx = 0;
                dev.currentByte = 0;
            }
        }
        break;

    case OWI_STATE_SEARCH_ROM:
        // Search ROM happens LSB first, bit by bit.
        // Each 'bit' cycle starts with the Master pulling the line low (level == 0)
        if (level == 0)
        { // searchROMstate 0: Send Real Bit
            // searchROMstate 1: Send Inverted Bit
            // searchROMstate 2: Receive Master's direction
            uint8_t currentRomBit = (dev.rom >> dev.bitCnt) & 0x01;
            // evt.evt = 11;
            // evt.level = currentRomBit;
            // evt.cnt = dev.searchROMstate;
            // xQueueSendToBackFromISR(printQueue, &evt, NULL);

            switch (dev.searchROMstate)
            {
            case 0: // 1. Send Real Bit
                if (currentRomBit == 0)
                {
                    // rmt_transmit(owi_tx_ch, copy_enc, &bit0_symbol, sizeof(bit0_symbol), &owi_rmt_tx_config);
                    owi_slave_write_zero();
                }
                dev.searchROMstate = 1;
                break;
            case 1: // 2. Send Inverted Bit
                if (currentRomBit == 1)
                {
                    // rmt_transmit(owi_tx_ch, copy_enc, &bit0_symbol, sizeof(bit0_symbol), &owi_rmt_tx_config);
                    owi_slave_write_zero();
                }
                dev.searchROMstate = 2;
                break;
            case 2: // Skip edge
                dev.searchROMstate = 3;
                break;
            default:
                break;
            }
        }
        else if (level == 1 && dev.searchROMstate == 3)
        {   // 3. Sample Master's Direction
            uint8_t masterChoice = (duration_us < 35) ? 1 : 0;
            uint8_t currentRomBit = (dev.rom >> dev.bitCnt) & 0x01;
            // evt.evt = 11;
            // evt.level = currentRomBit;
            // evt.cnt = dev.searchROMstate;
            // xQueueSendToBackFromISR(printQueue, &evt, NULL);

            // evt.evt = 10;
            // evt.cnt = masterChoice;
            // evt.level = currentRomBit;
            // xQueueSendToBackFromISR(printQueue, &evt, NULL);

            if (masterChoice != currentRomBit)
                device_reset(); // We don't match the Master's path
            else
            { // Match! Move to next bit
                dev.bitCnt++;
                dev.searchROMstate = 0; // Reset for next bit

                if (dev.bitCnt >= 64)
                { // Search finished, this slave is selected!
                    dev.state = OWI_STATE_RECEIVING_COMMAND;
                    dev.bitCnt = 0;
                }
            }
        }
        break;

    case OWI_STATE_RECEIVE_ROM:
        if (level == 1)
        {
            uint8_t bit = (duration_us < 35) ? 1 : 0;
            // evt.evt = 6;
            // evt.level = bit;
            // xQueueSendToBackFromISR(printQueue, &evt, NULL);

            dev.currentByte |= (bit << dev.bitCnt);
            dev.bitCnt++;

            if (dev.bitCnt == 8)
            {
                if (dev.currentByte != dev.romBytes[dev.byteIdx])
                    device_reset();
                else
                {
                    dev.byteIdx++;
                    dev.bitCnt = 0;
                    dev.currentByte = 0;

                    if (dev.byteIdx >= 8)
                    {
                        dev.state = OWI_STATE_RECEIVING_COMMAND;
                        evt.evt = 8;
                        xQueueSendToBackFromISR(printQueue, &evt, NULL);
                    }
                }
            }
        }
        break;

    case OWI_STATE_TRANSMITTING_DATA:
        if (dev.txBuffer == NULL)
            device_reset();
        if (level == 0)
        {
            uint8_t bit = (dev.txBuffer[dev.byteIdx] >> dev.bitCnt) & 0x01;
            // evt.evt = 6;
            // evt.level = bit;
            // xQueueSendToBackFromISR(printQueue, &evt, NULL);
            if (bit == 0)
            {
                // rmt_transmit(owi_tx_ch, copy_enc, &bit0_symbol, sizeof(bit0_symbol), &owi_rmt_tx_config);
                owi_slave_write_zero();
            }

            dev.bitCnt++;
            if (dev.bitCnt == 8)
            {
                dev.bitCnt = 0;
                dev.byteIdx++;
            }
            if (dev.byteIdx >= dev.txLen)
                device_reset();
        }
        break;
    default:
        break;
    }
}
