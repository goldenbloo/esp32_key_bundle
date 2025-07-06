#include "esp_log.h"
#include "esp_littlefs.h"
#include "rfid.h"
#include "menus.h"
#include "littlefs_records.h"
#include "math.h"
#include "string.h"

static const char *TAG = "littlefs_records";
const uint8_t zero_bssid[6] = {0};

void littlefs_init()
{
        esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    else
        ESP_LOGI(TAG, "LittleFS Partition size: total: %d, used: %d", total, used);

}

bool append_location(const location_t *loc) 
{
    FILE *f = fopen("/littlefs/locations.bin", "ab");  // append-binary
    if (!f) 
    {
        ESP_LOGE(TAG, "Failed to open for append");
        return false;
    }
    size_t written = fwrite(loc, sizeof(location_t), 1, f);
    fclose(f);
    ESP_LOGI(TAG, "Appended %d record", written);
    return (written == 1);
}

bool overwrite_location(const location_t *loc)
{
    FILE *f = fopen("/littlefs/locations.bin", "r+b");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for overwrite");
        return false;
    }
    long offset = (long)(loc->id - 1) * sizeof(location_t);
    if (fseek(f, offset, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "Failed to seek to record %ld", loc->id);
        fclose(f);
        return false;
    }

    size_t written = fwrite(loc, sizeof(location_t), 1, f);
    fclose(f);

    if (written == 1)
    {
        ESP_LOGI(TAG, "Overwrote record %ld, Name = %s", loc->id, loc->name);
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Write error at record %ld", loc->id);
        return false;
    }
}

void clear_all_locations() 
{
    FILE *f = fopen("/littlefs/locations.bin", "wb");  // write-binary
    if (!f) 
    {
        ESP_LOGE(TAG, "Failed to open for write");
        return;
    }    
    fclose(f);
    ESP_LOGI(TAG, "Location cleared");
    
}

bool read_all_locations(void) 
{
    FILE *f = fopen("/littlefs/locations.bin", "rb");
    if (!f) return false;
    ESP_LOGI(TAG, "Trying to dump locations");
    location_t loc;    
    while (fread(&loc, sizeof(location_t), 1, f) == 1) 
    {
        ESP_LOGI(TAG, "ID=%lu\tName=%s\ttag=0x%02x%02x%02x%02x%02x", loc.id, loc.name, loc.tag[4], loc.tag[3], loc.tag[2], loc.tag[1], loc.tag[0]);
        for (int i = 0; i < BSSID_MAX; i++)
        {
            if (memcmp(loc.bssids[i], zero_bssid, sizeof(zero_bssid) != 0))
            ESP_LOGI(TAG, "\t%02x:%02x:%02x:%02x:%02x:%02x %d", 
                loc.bssids[i][0], loc.bssids[i][1], 
                loc.bssids[i][2], loc.bssids[i][3],
                loc.bssids[i][4], loc.bssids[i][5],
                     loc.rssis[i]);
        }
        // you could dump BSSIDs/RSSIs here
    }
    fclose(f);
    return true;
}

bool find_best_location(uint8_t query_bssid[][6], int8_t query_rssi[], uint8_t query_ap_count, location_t *best_loc) {
    FILE *f = fopen("/littlefs/locations.bin", "rb");
    if (!f) return false;
    location_t loc;
    uint32_t bestDist = UINT32_MAX;
    bool found = false;

    while (fread(&loc, sizeof(location_t), 1, f) == 1)
    {
        
        int32_t dist = 0;
        for (int i = 0; i < query_ap_count; i++) // query loop
        {
            bool apMatched = false;
            for (int j = 0; j < BSSID_MAX; j++) // loc loop
            {
                if (memcmp(loc.bssids[j], zero_bssid, 6) == 0) 
                    continue;

                if (memcmp(loc.bssids[j], query_bssid[i], 6) == 0)
                {
                    int32_t diff = loc.rssis[j] - query_rssi[i];
                    dist += diff * diff;
                    apMatched = true;
                    break;
                }
            }
            if (!apMatched)
            {
                const int8_t  FLOOR_RSSI = -100;
                int32_t diff = FLOOR_RSSI - query_rssi[i];
                dist += diff * diff;
            }
        }
        if (dist < bestDist)
        {
            bestDist = dist;
            *best_loc = loc;
            found = true;
        }
    }
    fclose(f);
    return found;
}

// Return num of locations
int32_t find_multiple_best_locations(uint8_t query_bssid[][6], int8_t query_rssi[], uint8_t query_ap_count, location_t *bestLoc)
{
    FILE *f = fopen("/littlefs/locations.bin", "rb");
    if (!f) return 0;

    uint32_t selectedIds[LOC_MATCH_MAX] = {0};
    int foundIdCount = 0;

    // For each “slot” in the bestLoc[] array...
    for (int match = 0; match < LOC_MATCH_MAX; match++)
    {
        rewind(f);
        uint32_t bestLocalDist = UINT32_MAX;
        bool foundLoc = false;
        location_t loc;

        // scan every stored location
        while (fread(&loc, sizeof(location_t), 1, f) == 1)
        {
            // skip ones we've already selected
            bool skip = false;
            for (int k = 0; k < match; k++)
                if (loc.id == selectedIds[k]) { skip = true; break; }
            if (skip) continue;

            // compute squared Euclidean distance over the query APs
            uint32_t dist = 0;
            for (int i = 0; i < query_ap_count; i++)
            {
                bool matched = false;
                // look for the same BSSID in this loc
                for (int j = 0; j < BSSID_MAX; j++)
                {
                    if (memcmp(loc.bssids[j], zero_bssid, 6) == 0)
                        break; // no more valid entries

                    if (memcmp(loc.bssids[j], query_bssid[i], 6) == 0)
                    {
                        int32_t diff = (int32_t)loc.rssis[j] - query_rssi[i];
                        dist += (uint32_t)(diff * diff);
                        matched = true;
                        break;
                    }
                }
                if (!matched)
                {
                    // penalty for missing AP: assume a floor RSSI (e.g. –100 dBm)
                    const int8_t FLOOR_RSSI = -100;
                    int32_t diff = (int32_t)FLOOR_RSSI - query_rssi[i];
                    dist += (uint32_t)(diff * diff);
                }
            }

            // keep the location with the SMALLEST dist
            if (dist < bestLocalDist)
            {
                bestLocalDist = dist;
                bestLoc[match] = loc;
                foundLoc = true;
            }
        }

        if (!foundLoc)
            break; // no more candidates

        selectedIds[match] = bestLoc[match].id;
        foundIdCount++;
    }

    fclose(f);
    return foundIdCount;
}

int32_t find_best_locations_from_scan(const wifi_ap_record_t* ap_records, int ap_count, location_t* bestLocs_out, bool singleMatch)
{
    const char *TAG = "find_locations";
    int locNum = 0;
    if (ap_count == 0) return 0; // Nothing to process    

    // 1. Allocate temporary memory for BSSIDs and RSSIs
    uint8_t (*bssids)[6] = malloc(sizeof(*bssids) * ap_count);
    int8_t *rssis = malloc(sizeof(*rssis) * ap_count);

    if (!bssids || !rssis)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for Wi-Fi data extraction");
        free(bssids); // free is safe to call on NULL
        free(rssis);
        return -1; // Return an error code
    }

    // 2. Extract the data from the records
    for (int i = 0; i < ap_count; i++)
    {
        memcpy(bssids[i], ap_records[i].bssid, sizeof(ap_records[i].bssid));
        rssis[i] = ap_records[i].rssi;
    }

    // 3. Call the core logic function
    if (singleMatch) 
    {
        bool found = find_best_location(bssids, rssis, ap_count, &bestLocs_out[0]);
        locNum = found ? 1 : 0;
    }
    else
        locNum = find_multiple_best_locations(bssids, rssis, ap_count, bestLocs_out);
    // 4. Clean up the temporary memory
    free(bssids);
    free(rssis);

    return locNum;
}

int32_t get_next_location_id(void)
{
    FILE *f = fopen("/littlefs/locations.bin", "rb");
    if (!f)
    {  // File doesn't exist yet; start IDs from 0
        return 0;
    }
    // Seek to the last record
    if (fseek(f, -((long)sizeof(location_t)), SEEK_END) != 0)
    {
        fclose(f);
        return 1;
    }
    location_t last;
    if (fread(&last, sizeof(location_t), 1, f) != 1)
    {
        fclose(f);
        ESP_LOGE(TAG,"last loc read failed");
        return -1;
    }
    fclose(f);
    return last.id + 1;
}

int find_locations_by_name(const char *substr, location_t results[], int max_results)
{
    if (substr == NULL || *substr == '\0' || max_results <= 0) {
        return 0;
    }

    FILE *f = fopen("/littlefs/locations.bin", "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open locations file");
        return 0;
    }

    int idx = 0;
    location_t loc;

    while (idx < max_results
           && fread(&loc, sizeof(location_t), 1, f) == 1)
    {
        
        if (strcasestr(loc.name, substr) != NULL)
        {
            results[idx++] = loc;
        }
    }

    fclose(f);
    return idx;
}