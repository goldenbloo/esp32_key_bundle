#ifndef LFS_RECORDS_H
#define LFS_RECORDS_H

#include "types.h"
#include "esp_wifi_types.h"

extern key_data_t currentKeyData;
extern uint16_t currentKeyType;

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