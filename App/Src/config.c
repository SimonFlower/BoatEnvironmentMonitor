/** Functions to manage the program's configuration, which is stored in
 * flash memory */

#include <stdbool.h>
#include <string.h>

#include "stm32l4xx_hal.h"

#include "utils.h"
#include "config.h"

// These are defined in the linker script (CubeMX/STM32L432xx_FLASH.ld).
// The address of each is where the configuration area in flash memory
// starts and ends.
extern uint8_t __config_start__[];
extern uint8_t __config_end__[];

// The address of the start of the configuration area in flash
// memory and the length of the area.
#define CONFIG_FLASH_ADDR ((void *)__config_start__)
#define CONFIG_FLASH_SIZE ((size_t)(__config_end__ - __config_start__))

// The flash page number corresponding to this address
#define CONFIG_FLASH_PAGE ((((uint32_t) CONFIG_FLASH_ADDR) - FLASH_BASE) / FLASH_PAGE_SIZE)

// A copy of the program configuration that's stored in flash memory
static ConfigData_t config_data;

/**
 * @brief Load the configuration data from flash memory
 * 
 * This function returns a pointer to a static configuration data
 * structure. Callers can modify the structure, then call SaveConfig
 * to store the updated values back to flash memory.
 * 
 * @retval The configuration data
 */
ConfigData_t *GetConfig (void) {
	static bool first = true;
	
	// On the first call, load the configuration data from flash memory
	if (first) {
		// Copy the configuration data from flash memory - flash memory
		// is already memory mapped and can be read without any HAL calls.
		memcpy (&config_data, (const void *)&__config_start__, sizeof (ConfigData_t));
		
		// Check the magic number on the configuration data
		if (config_data.magic_no == CONFIG_MAGIC_NUMBER) {
			config_data.loaded_from_flash = true;
		} else {
		    memset(&config_data, 0, sizeof config_data);
			// If the configuration data wasn't retrieved, use defaults
			config_data.magic_no = CONFIG_MAGIC_NUMBER;
			config_data.debug_level = 5;
			config_data.loaded_from_flash = false;
			strncpy_term (config_data.apn, "TM", CFG_APN_LEN);
			strncpy_term (config_data.htp_hosts[0], "www.google.com", CFG_HTP_HOST_LEN);
			strncpy_term (config_data.htp_hosts[1], "www.cloudflare.com", CFG_HTP_HOST_LEN);
			strncpy_term (config_data.htp_hosts[2], "aws.amazon.com", CFG_HTP_HOST_LEN);
			strncpy_term (config_data.htp_hosts[3], "www.microsoft.com", CFG_HTP_HOST_LEN);
			strncpy_term (config_data.dht22_names[0], "Temperature / Humidity 1", CFG_DHT22_NAME_LEN);
			strncpy_term (config_data.dht22_names[1], "Temperature / Humidity 2", CFG_DHT22_NAME_LEN);
			strncpy_term (config_data.dht22_names[2], "Temperature / Humidity 3", CFG_DHT22_NAME_LEN);
			strncpy_term (config_data.dht22_names[3], "Temperature / Humidity 4", CFG_DHT22_NAME_LEN);
			strncpy_term (config_data.dht22_names[4], "Temperature / Humidity 5", CFG_DHT22_NAME_LEN);
			strncpy_term (config_data.battery_adc_names [0], "Battery 1", CFG_BATTERY_ADC_NAME_LEN);
			strncpy_term (config_data.battery_adc_names [1], "Battery 2", CFG_BATTERY_ADC_NAME_LEN);
			strncpy_term (config_data.battery_adc_names [2], "Battery 3", CFG_BATTERY_ADC_NAME_LEN);
		}
			
		first = false;
	}

	// Return a pointer to the static configuration data
	return &config_data;
}

/**
 * @brief Store the configuration data in flash memory
 * @retval true if the data was stored OK
 */
bool SaveConfig (void) {
    HAL_FLASH_Unlock();

    // Clear any previous error flags
    __HAL_FLASH_CLEAR_FLAG (FLASH_FLAG_ALL_ERRORS);

    // Erase page
	// This assumes that the configuration data occupies one page of flash memory
    FLASH_EraseInitTypeDef erase;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = FLASH_BANK_1;
    erase.Page      = CONFIG_FLASH_PAGE;
    erase.NbPages   = 1;

    uint32_t page_error;
    HAL_StatusTypeDef rc = HAL_FLASHEx_Erase(&erase, &page_error);
    if (rc != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    // Copy to flash in 64-bit doublewords, aligned on a 64-bit boundary
	const uint8_t *src = (const uint8_t *) &config_data;
	size_t remaining = sizeof(ConfigData_t);
	uint32_t addr = (uint32_t) CONFIG_FLASH_ADDR;
	while (remaining > 0) {
		uint64_t dw = UINT64_MAX;            // all 0xFF = erased state
		size_t n = (remaining < 8) ? remaining : 8;
		memcpy(&dw, src, n);                 // copies 8 bytes, or fewer on the last chunk
		rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, dw);
		if (rc != HAL_OK) {
			HAL_FLASH_Lock();
			return false;
		}
		src       += n;
		addr      += 8;
		remaining -= n;
	}

	// Finished erasing and writing to flash
    HAL_FLASH_Lock();
    
    // Compare the data in flash with the data in volatile memory
    if (memcmp(&config_data, (const void *)&__config_start__, sizeof (ConfigData_t)) != 0)
		return false;

    return true;
}

/**
 * @brief Get the address of the start of the configuration area in flash memory
 * @retval A pointer to the memory location
 */
void *GetConfigMemAddr (void) {
	return CONFIG_FLASH_ADDR;
}

/**
 * @brief Get the size of of the configuration area in flash memory
 * @retval The size in bytes
 */
size_t GetConfigMemSize (void) {
	return CONFIG_FLASH_SIZE;
}
