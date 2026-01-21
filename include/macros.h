
#define RFID_RX             4
#define LED_PIN             2
#define RFID_CLK_DATA       18
#define COIL_VCC_PIN        5
#define BUTTON_PIN          23
#define CONFIG_SDA_GPIO     21
#define CONFIG_SCL_GPIO     22

#define CD4017_CLK          14
#define CD4017_RST          12
#define KEYPAD_C0           27
#define KEYPAD_C1           26

#define METAKOM_TX          32
#define COMP_RX             35
#define OWI_TX              33
#define PULLUP_PIN          25
 

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