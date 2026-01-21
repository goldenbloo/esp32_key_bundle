#ifndef LFS_RECORDS_H
#define LFS_RECORDS_H




typedef union {
    uint64_t value;          
    uint8_t bytes[8];
    struct { uint64_t rom; } ds1990;
    struct { uint8_t  id[5]; } rfid;
    struct { uint32_t id; } kt2;
} key_data_t;

typedef enum
{
  KEY_TYPE_NONE,
  KEY_TYPE_RFID,
  KEY_TYPE_KT2,
  KEY_TYPE_DALLAS,

} key_type_enum;

typedef struct __attribute__((packed))
{
  key_data_t keyData;
  int32_t id;  
  uint16_t keyType;
  uint8_t bssids[5][6];
  int8_t rssis[5];
  char name[FIELD_SIZE];
} location_t;





void littlefs_init();

// bool find_best_location(uint8_t query_bssid[][6], int8_t query_rssi[], uint8_t query_ap_count, location_t *best_out);
// int32_t find_multiple_best_locations(uint8_t query_bssid[][6], int8_t query_rssi[], uint8_t query_ap_count, location_t *bestLoc);
int32_t find_best_locations_from_scan(const wifi_ap_record_t* ap_records, int ap_count, location_t* bestLocs_out, bool singleMatch);
bool append_location(const location_t *loc);
int32_t get_next_location_id(void);
bool read_all_locations(void);
void clear_all_locations();
bool write_location(const location_t *loc);
int32_t find_locations_by_name(const char *substr, location_t results[], int max_results);
bool delete_location(int32_t id_to_delete);

#endif