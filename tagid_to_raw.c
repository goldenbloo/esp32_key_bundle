// Online C compiler to run C program online
#include <stdio.h>
#include <stdint.h>

char str[65];

const uint8_t parity_row_table[16] = {
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0};
    
char *int_to_char_bin(char* str, uint64_t num)
{
    for (uint8_t i = 0; i < 64; i++)
    {
        str[63 - i] = ((num >> i) & 1) + 0x30;
    }
    str[64] = 0;
    return str;
}
uint64_t tagId_to_raw_tag(uint8_t *tagArr)
{
    uint64_t rawTag = 0x1FF;
    uint8_t nibbles[11]; // 10 nibbles 4 bits - data, 1 bit - parity;  last nibble 4 bits - column parity and 1 bit - stop
    uint8_t column_parity = 0;
    for (uint8_t i = 0; i < 10; i++)
    {
        uint8_t nibble = (tagArr[i / 2] >> 4 * (i % 2)) & 0xF;
        nibbles[9-i] = (nibble << 1) | parity_row_table[nibble];
        column_parity ^= nibble; 
    }
   nibbles[10] &= column_parity << 1; // Clear bit 0 for stop bit

    for (uint8_t i = 0; i <11; i++)
    {
        printf("tag raw: %s\n", int_to_char_bin(str, rawTag));
        rawTag = (rawTag << 5) | (nibbles[i] & 0x1F);        
    }
    printf("tag raw: %s\n", int_to_char_bin(str, rawTag));
    return rawTag;
}



int main()
{
    //                   0    1    2    3    4 
    uint8_t tagId[5] = {0x2c,0xb6,0x39,0x00,0x18};
    uint64_t rawtag = tagId_to_raw_tag(tagId);
    // printf("tagmraw: %s", int_to_char_bin(str, rawtag));
    uint64_t long_tag = 0;
    for (uint8_t i = 0; i < 5; i++)
    {
        long_tag = long_tag | (uint64_t)(tagId[i] << 8 * i);
        printf("longtag %#lx\n",long_tag);
    }
    
}