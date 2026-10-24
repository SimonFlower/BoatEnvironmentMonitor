/** Manage taking ADC readings. */

#include <stdint.h>
#include <stdbool.h>

#include "stm32l4xx_hal.h"

#include "user_io.h"
#include "adc.h"

// A flag to show that the conversion has completed
static volatile bool adc_complete = false;

// A flag to show that there was a conversion error
static volatile bool conv_err = false;

// The controlling structure for ADC1
extern ADC_HandleTypeDef hadc1;

/**
 * @brief Take a set of ADC readings
 * @param timeout The time to wait for the readings, in mS
 * @param readings an array of 4 integers to put the readings into
 * @retval true if the readings were retrieved OK
 */
bool ADCTakeReadings (int timeout, uint16_t *readings) {
	adc_complete = false;
	conv_err = false;
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *) readings, 4);

	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < timeout) {
		if (adc_complete) {
			if (conv_err) {
				DebugPrint (2, true, true, "ADC conversion error");
				return false;
			}
			DebugPrint (2, true, true, "ADC readings: %u %u %u %u",
						*readings, *(readings + 1),
						*(readings + 2), *(readings + 3));
			return true;
		}
	}
	DebugPrint (2, true, true, "Timeout waiting for ADC");
	return false;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		adc_complete = true;
	}
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
		conv_err = true;
		adc_complete = true;
    }
}
