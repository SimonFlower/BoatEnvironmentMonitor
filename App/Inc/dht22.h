/* routines for reading from the DHT22 temperature and humidity sensor */

#ifndef DHT22_H
#define DHT22_h

/** There are 5 DHT22 sensors */
typedef enum { DHT22_1, DHT22_2, DHT22_3, DHT22_4, DHT22_5, N_DHT22_SENSORS} DHT22SensorID_t;

/** Return values from DHT22TakeReading() */
typedef enum { DHT22_READING_OK, DHT22_NO_SENSOR, DHT22_ERROR } DHT22ReadingStatus_t;

/** a structure to hold a reading from a DHT22 sensor */
typedef struct {
	DHT22SensorID_t id;
	bool status;		// true if readings are usable
	int temperature;	// temperature in C, multiplied by ten
	int humidity;		// humidity as a percentage, multipled by ten
} DHT22Reading_t;

// forward declarations
bool DHT22Init (void);
DHT22ReadingStatus_t DHT22TakeReading (DHT22SensorID_t dht22_id, DHT22Reading_t *reading);

#endif /* DHT22_H */
