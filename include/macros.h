
#define RFID_RX             4
#define LED_PIN             2
#define RFID_CLK_DATA       5
#define COIL_VCC_PIN        18
#define CONFIG_SDA_GPIO     21
#define CONFIG_SCL_GPIO     22

#define CD4017_CLK          13
#define CD4017_RST          12
#define KEYPAD_C0           14
#define KEYPAD_C1           27

#define METAKOM_TX          25
#define COMP_RX             32
#define PULLUP_PIN          33
#define OWI_BUS_DRIVE       26

#if CONFIG_OWI_LOOPBACK_TEST       // single-ESP32 master+slave testing
#define OWI_MASTER_DRIVE    34   // separate test pin wired to same bus
#else
#define OWI_MASTER_DRIVE    OWI_BUS_DRIVE
#endif
#define OWI_SLAVE_DRIVE     OWI_BUS_DRIVE
 

#define PERIOD              512
#define PERIOD_LOW          387
#define PERIOD_HIGH         640
#define PERIOD_HALF         256
#define PERIOD_HALF_LOW     140
#define PERIOD_HALF_HIGH    386
#define RMT_SIZE            64


#define MUL_BY_0_8(x)       (((x) * 52429) >> 16)
#define MUL_BY_0_75(x)      (((x) * 49152) >> 16)
#define MUL_BY_1_2(x)       (((x) * 78643) >> 16)
#define MUL_BY_1_25(x)      (((x) * 81920) >> 16)

#define FIELD_SIZE          30
#define MENU_STACK_SIZE     10
#define BSSID_MAX           5
#define LOC_NUM_MAX         5
#define LOC_SEARCH_MAX      5
#define LOC_MATCH_MAX       3

#define MENU_STACK_MAX_SIZE 10