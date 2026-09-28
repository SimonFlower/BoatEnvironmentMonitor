/** management of the CubeMX IWDG (Independent Watchdog)
 * 
 * HAL_IWDG_Refresh should not be called until the IWDG has been
 * initialised. Code in CubeMX/Core/SRC/iwdg.c calls
 * setIWDGInitialise() when initialisation is complete.
 * The application should call PingIWDG instead of
 * HAL_IWDG_Refresh.
 */

#include <stdbool.h>

#include "stm32l4xx_hal.h"

#include "app_iwdg.h"

static bool IWDG_Initialised = false;

// defined in CubeMX/Core/SRC/iwdg.c
extern IWDG_HandleTypeDef hiwdg;

void SetIWDGInitialised (void) {
	IWDG_Initialised = true;
}

void PingIWDG (void) {
	if (IWDG_Initialised)
		HAL_IWDG_Refresh(&hiwdg);
}
