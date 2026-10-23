#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "stm32l4xx_hal.h"

// an identifier used to check the validity of the configuration data
#define CONFIG_MAGIC_NUMBER	0x1f2e3d4e

// sizes for the string fields in the configuration data
#define CFG_APN_LEN					64
#define CFG_N_HTP_HOSTS				4
#define CFG_HTP_HOST_LEN			64
#define CFG_N_DHT22_SENSORS			5
#define CFG_DHT22_NAME_LEN			64
#define CFG_N_BATTERY_ADCS			3
#define CFG_BATTERY_ADC_NAME_LEN	64

// The configuration data that is written to flash memory
typedef struct {
	// an identifier used to check the validity of the data
	uint32_t magic_no;
	// the amount of debugging inormation to log
	uint8_t debug_level;
	// a flag that shows whether the configuration was loaded 
	// from flash or used default values - this is status rather
	// than configuration
	bool loaded_from_flash;
	// the mobile network Access Point Name
	char apn [CFG_APN_LEN];
	// IP names or addresses of HTP (time synchronisation) hosts
	char htp_hosts [CFG_N_HTP_HOSTS] [CFG_HTP_HOST_LEN];
	// names of DHT22 temperature / humidity sensors
	char dht22_names [CFG_N_DHT22_SENSORS] [CFG_DHT22_NAME_LEN];
	// names of batteries
	char battery_adc_names [CFG_N_BATTERY_ADCS] [CFG_BATTERY_ADC_NAME_LEN];
} ConfigData_t;

// Check that the configuration data will fit into the designated area in flash memory.
// This assumes that the configuration data occupies one page of flash memory.
// This is set in the patch_cubemx_link_script.sh script, where a new CONFIG memory
// region is defined. If this assert fails (and more flash memory is needed for the
// configuration) then change the settings in patch_cubemx_link_script.mx to allocate
// mutiple pages of memory to the config area and update the comparison below to
// use the appropriate integer multiple of FLASH_PAGE_SIZE
_Static_assert(sizeof(ConfigData_t) <= FLASH_PAGE_SIZE, "ConfigData_t too large for flash memory space");

// Forward declarations
void SetConfigDefaults (void);
ConfigData_t *GetConfig (void);
bool SaveConfig (void);
void *GetConfigMemAddr (void);
size_t GetConfigMemSize (void);

#endif /* CONFIG_H */
