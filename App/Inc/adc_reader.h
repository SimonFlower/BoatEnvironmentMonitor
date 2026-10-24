/** Manage taking ADC readings. */

#ifndef ADC_READER_H
#define ADC_READER_H

#include <stdint.h>
#include <stdbool.h>

// Forward declations
bool ADCTakeReadings (int timeout, uint16_t *readings);

#endif /* ADC_READER_H */
