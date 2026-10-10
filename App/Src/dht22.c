/** Functions to manage the DHT22 temperature and humidity sensors 
 * 
 * The DHT22 works like this. The pin it connects to on the Nucleo L432KC board
 * must be defined as an open drain output, default high. An external 4K7
 * resistor should be used as a pull up to the 3.3V supply.
 * 
 * To start a conversion, pull the pin low for at least 1mS, then high for
 * at least 20uS and start reading the pin. The DHT22 will:
 *   - pull the line low for 80uS
 *   - pull the line high for 80uS
 * The DHT22 will then send 40 bits of data. Each bit consists of:
 *   - pull the line low for 50uS
 *   - pull the line high for:
 * 		- 26-28uS for a '0'
 *      - 70uS for a '1'
 * At the end, the line is pulled low for 50uS, then left high.
 * 
 * In total there should be 2 + (40 x 2) + 2 'edges' = 84
 * 
 * The 40 bits of data are organised as:
 *   - 16 bits of relative humidity data, in %, multiplied by ten, MSB first
 *   - 16 bits of temperature data, in C, multiplied by ten, MSB first
 * 	   Exclude the top bit of the first byte from the calculation of the value
 *     If the top bit of the first byte is set, the value is negative
 *   - 8 bits of checksum. The checksum is calculated as the lowest
 *       8 bits of the addition of the 4 previous bytes
 * 
 * This code assumes that the TIM2 timer has been configured to run at
 * 1Mhz.
 * 
 * */

#include <stdio.h>
#include <stdbool.h>

#include "stm32l4xx_hal.h"
#include "main.h"

#include "debug.h"
#include "dht22.h"

// Defined in ./CubeMX/Core/Src/tim.c
extern TIM_HandleTypeDef htim2;

// DHT22 timing constants
#define IDLE_TIMEOUT_TICKS   200        // no edge for 200 us => frame over
#define FRAME_TIMEOUT_TICKS  10000      // 10 ms overall limit
#define RESP_LOW_IDX     1              // Index of initial frame of ~80uS low
#define RESP_HIGH_IDX    2              // Index of subsequent frame of ~80uS high
#define FIRST_DATA_PULSE 3              // Index of first data pulse
#define N_DATA_FRAMES    40			    // number of data frames in the input stream
#define MIN_EDGES        83             // expect 84; last (rising) edge optional
#define MAX_EDGES        90				// the maximum number of pulse edges to capture

// flag the DHT22Init has been called successfully
static bool dht22_init_ok = false;

// private forward declarations
static inline void delay_us(uint32_t us);


/**
 * @brief configure the hardware for DHT22 readings
 * @return true if the clock used for DHT22 timing started OK
 **/
bool DHT22Init (void) {
	dht22_init_ok = false;
	
	// The reading routines use timer TIM2
	if (HAL_TIM_Base_Start(&htim2) != HAL_OK) {
#if DEBUG >= 2
		printf ("DHT22 init: failed to start TIM2\r\n");
#endif
		return false;
	}
	
	// The DHT22 needs two seconds from power on before taking its first reading
	// during this delay, check that TIM2 is running
    __HAL_TIM_SET_COUNTER(&htim2, 0);
	HAL_Delay (2000);
	uint32_t us = __HAL_TIM_GET_COUNTER(&htim2);
	uint32_t expected = 2000000;
	if (us < (expected - (expected / 20)) || us > (expected + (expected / 20))) {
#if DEBUG >= 2
		if (us == 0)
			printf ("DHT22 init: TIM2 not running\r\n");
		else
			printf ("DHT22 init: TIM2 running at wrong speed (%lu should be %lu)\r\n",
			        us, expected);
#endif
		return false;
	}

	dht22_init_ok = true;
	return true;
}

/**
 * @brief Take readings from the DHT22 sensors
 * 
 * Note that individual DHT22 sensors require a minimum 2 second gap
 * between readings.
 * 
 * @param dht22_id the ID of the sensor to read
 * @param reading the results of the reading
 * @retval true if the reading was made OK
 */
DHT22ReadingStatus_t DHT22TakeReading (DHT22SensorID_t dht22_id, DHT22Reading_t *reading) {

	static uint32_t pulse_width [MAX_EDGES];
	
	reading->id = dht22_id;
	reading->status = false;
	reading->temperature = 0;
	reading->humidity = 0;
	
	if (! dht22_init_ok || ! reading)
		return DHT22_ERROR;

	// Work out which pin we want to use (all on GPIO B)
	uint32_t pin_mask;
	switch (dht22_id) {
		case DHT22_1: pin_mask = GPIO_PIN_0; break;
		case DHT22_2: pin_mask = GPIO_PIN_4; break;
		case DHT22_3: pin_mask = GPIO_PIN_5; break;
		case DHT22_4: pin_mask = GPIO_PIN_6; break;
		case DHT22_5: pin_mask = GPIO_PIN_7; break;
		default: return DHT22_ERROR;
	}

    // Drive pin LOW to initiate start signal (1.2 ms duration)
    GPIOB->BSRR = pin_mask << 16;
    delay_us(1200);

	// disable IRQs for the duration of the capture to prevent timing errors
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    // Release pin HIGH (High-Z via Open-Drain, pulled up externally)
    GPIOB->BSRR = pin_mask;

    // Wait for the pull-up to bring the line high - if it doesn't it's
    // probably because a sensor isn't connected
    uint32_t t_begin = TIM2->CNT;
    while (!(GPIOB->IDR & pin_mask)) {
        if ((TIM2->CNT - t_begin) > 50) {
            __set_PRIMASK(primask);
#if DEBUG >= 2
			printf ("DHT22 sensor ID %d: timeout waiting for line to go high after start pulse\r\n",
			        (int) dht22_id);
#endif
            return DHT22_NO_SENSOR;
        }
    }

	// Capture the signal from the DHT22 by timing the edges of the pulses being sent
    uint32_t last_pin_val = pin_mask;
    uint32_t t_last_edge = TIM2->CNT;
    int n_edges = 0;
    while (n_edges < MAX_EDGES) {
        uint32_t now_pin_val = GPIOB->IDR & pin_mask;
        uint32_t t = TIM2->CNT;
        if (now_pin_val != last_pin_val) {
            pulse_width [n_edges ++] = t - t_last_edge;
            last_pin_val = now_pin_val;
            t_last_edge = t;
        } else if ((t - t_last_edge) > IDLE_TIMEOUT_TICKS ||
                   (t - t_begin) > FRAME_TIMEOUT_TICKS) {
            break;
        }
    }
    // Re-enable interrupts
    __set_PRIMASK(primask);

	// Debug dump of the pulse buffer
#if DEBUG >= 3
	printf ("DHT22 sensor ID %d, dump pulse buffer, %d entries:", (int) dht22_id, n_edges);
	for (int count=0; count<n_edges; count++)
		printf (" %lu", pulse_width[count]);
	printf ("\r\n");
#endif
        
    // - Check all pulses received
    if (n_edges < MIN_EDGES) {
#if DEBUG >= 2
		printf ("DHT22 sensor ID %d, insufficient pulses (%d should be at least %d)\r\n",
				(int) dht22_id, n_edges, MIN_EDGES);
#endif
		return DHT22_ERROR;
	}
	
	// The initial framing pulses should be about 80uS
    if (pulse_width[RESP_LOW_IDX]  < 60 || pulse_width[RESP_LOW_IDX]  > 100 ||
        pulse_width[RESP_HIGH_IDX] < 60 || pulse_width[RESP_HIGH_IDX] > 100) {
#if DEBUG >= 2
		printf ("DHT22 sensor ID %d, framing pulse widths incorrect: %lu %lu\r\n",
				(int) dht22_id, pulse_width[RESP_LOW_IDX], pulse_width[RESP_HIGH_IDX]);
#endif
		return DHT22_ERROR;
	}

    // Extract the data bits, MSB first, checking each pulse width
    uint8_t data[5] = {0};
    for (int count = 0; count < N_DATA_FRAMES; count++) {
		int pulse_index = FIRST_DATA_PULSE + (2 * count);
        uint32_t low_width  = pulse_width[pulse_index];
        uint32_t high_width = pulse_width[pulse_index +1];
        if (low_width < 30 || low_width > 70 || high_width < 15 || high_width > 85) {
#if DEBUG >= 2
			printf ("DHT22 sensor ID %d, data pulse %d/%d, frame or data width incorrect: %lu %lu\r\n",
                    (int) dht22_id, pulse_index, pulse_index +1,
                    low_width, high_width);
#endif
            return DHT22_ERROR;
		}
        if (high_width > 40)
            data[count / 8] |= 0x80 >> (count % 8);
    }

    // Verify 8-bit checksum and finalise the data
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
#if DEBUG >= 2
		printf ("DHT22 sensor ID %d, checksum incorrect: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\r\n",
		        (int) dht22_id, 
		        (unsigned int) data[0], (unsigned int) data[1], (unsigned int) data[2],
		        (unsigned int) data[3], (unsigned int) data[4]);
#endif
		return DHT22_ERROR;
	}
			
	// Finalise the data
    reading->humidity = (data[0] << 8) | data[1];
    reading->temperature = ((data[2] & 0x7F) << 8) | data[3];
    // Handle negative temperature sign bit (MSB of byte 2)
    if (data[2] & 0x80)
        reading->temperature *= -1;
        
    // Final sanity check on the data
    if (reading->humidity < 0 || reading->humidity > 1000 ||
        reading->temperature < -400 || reading->temperature > 800) {
#if DEBUG >= 2
	printf ("DHT22 sensor ID %d, readings outside plausible values: temperature %d(10xC), humidity %d(10x%%)\r\n",
	        (int) dht22_id, reading->temperature, reading->humidity);
#endif

		return DHT22_ERROR;
	}

#if DEBUG >= 2
	printf ("DHT22 sensor ID %d, temperature %d(10xC), humidity %d(10x%%)\r\n",
	        (int) dht22_id, reading->temperature, reading->humidity);
#endif
	reading->status = true;
    return DHT22_READING_OK;
}

/**
 * @brief Microsecond delay helper using TIM2 counter.
 * 
 * Requires that the system clock is 32MHz and the prescalar
 * is set to 31, which gives a TIM2 rate of 1MHz, as checked by
 * DHT22Init()
 */
static inline void delay_us(uint32_t us) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}
