#ifndef LFS_RECORDS_H
#define LFS_RECORDS_H



typedef struct __attribute__((packed))
{
  int32_t id;
  char name[FIELD_SIZE];
  uint8_t bssids[5][6];
  int8_t rssis[5];
  uint8_t tag[5];
} location_t;


void littlefs_init();

bool find_best_location(uint8_t query_bssid[][6], int8_t query_rssi[], uint8_t query_ap_count, location_t *best_out);
int32_t find_multiple_best_locations(uint8_t query_bssid[][6], int8_t query_rssi[], uint8_t query_ap_count, location_t *bestLoc);
bool append_location(const location_t *loc);
int32_t get_next_location_id(void);
bool read_all_locations(void);
void clear_all_locations();


#endif