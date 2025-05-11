typedef struct
{
  bool isSynced;
  uint8_t currentBit;
  uint8_t lastBit;
  bool checkNextEdge ;
  bool bitIsReady;
  uint64_t tagInputBuff;
  
} manchester_t;

void inline syncErrorFunc(manchester_t* m)
  {
    m->checkNextEdge = false;
    m->bitIsReady = false;
    m->checkNextEdge = false;
    m->tagInputBuff = 0;
  }

char* int_to_char_bin(char* str, uint64_t num)
{
  for (uint8_t i = 0; i < 64; i++)
  {
    str[63 - i] = ((num >> i) & 1) + 0x30;
  }
  str[64] = 0;
  return str;
}