/**
 * @file arduino.omp
 * @author Daniel Starke
 * @copyright Copyright 2019 Daniel Starke
 * @date 2019-06-01
 * @version 2019-06-28
 */
#include "DHT11.hpp"
#include "Meta.hpp"
#ifdef __AVR__
#include <avr/power.h>
#endif /* __AVR__ */


/** Defines the PIN DHT11 is connected to. */
#define DHT11_PIN 20


/** Defines the 1st order calibration value for temperature (max resolution is 0.1f). */
#define DHT11_TEMP_CAL 0.0f


/** Defines the 1st order calibration value for RH (max resolution is 0.1f). */
#define DHT11_RH_CAL 0.0f


/** Update interval in milliseconds. Should be greater than or equal 1100. */
#define UPDATE_INTERVAL 2000UL


/** Last update time value of millis(). */
unsigned long last;


DEF_HAS_MEMBER(dtr)
DEF_HAS_MEMBER(getDTR)
template <typename T> typename enable_if<!has_member_dtr<T>::value, bool>::type serialDtr(T & ser) { return bool(ser); }
template <typename T> typename enable_if<has_member_dtr<T>::value, bool>::type serialDtr(T & ser) { return ser.dtr(); }
template <typename T> typename enable_if<!has_member_getDTR<T>::value, bool>::type serialGetDtr(T & ser) { return serialDtr(ser); }
template <typename T> typename enable_if<has_member_getDTR<T>::value, bool>::type serialGetDtr(T & ser) { return ser.getDTR(); }


/**
 * Checks if the given serial interface is connected to the host.
 * 
 * @param[in] ser - serial interface instance
 * @return true if connected, else false
 * @remarks The functions serialGetDtr() and serialDtr() are used to detect the target specific API for this check.
 * @tparam T - type of the serial interface instance
 */
template <typename T>
bool isSerialConnected(T & ser) {
	return serialGetDtr(ser);
}


/** Setup environment. */
void setup(void) {
#ifdef __AVR__
	/* disable unneeded components to save power */
	power_adc_disable();
	power_usart0_disable();
	power_usart1_disable();
	power_spi_disable();
	power_twi_disable();
	power_timer1_disable();
	power_timer2_disable();
	power_timer3_disable();
#endif /* __AVR__ */
#ifdef LED_BUILTIN_RX
	pinMode(LED_BUILTIN_RX, INPUT);
#endif /* LED_BUILTIN_RX */
#ifdef LED_BUILTIN_TX
	pinMode(LED_BUILTIN_TX, INPUT);
#endif /* LED_BUILTIN_TX */
	DHT11::begin(DHT11_PIN);
	Serial.begin(9600);
	/* wait for serial connection */
	while ( ! isSerialConnected(Serial) );
	last = millis();
}


/** Output temperature and humidity once every UPDATE_INTERVAL. */
void loop(void) {
	/* check update interval deadline */
	const unsigned long now = millis();
	const unsigned long diff = now - last;
	if (diff < UPDATE_INTERVAL) {
		delay(1);
		return;
	}
	if (diff > 0x7FFFFFFFUL) {
		last = now;
	} else {
		last += UPDATE_INTERVAL;
	}
	/* wait for serial connection */
	while ( ! isSerialConnected(Serial) ) return;
	/* retrieve and print sensor values */
	const DHT11::Result val = DHT11::read(DHT11_PIN);
	if ( ! isSerialConnected(Serial) ) return;
	if (val.res == DHT11::SUCCESS) {
		const float temp = val.getTemp() + DHT11_TEMP_CAL;
		const float rh   = val.getRH()   + DHT11_RH_CAL;
		const float sum  = temp + rh; /* checksum */
		Serial.print(temp, 1);
		Serial.print('\t');
		Serial.print(rh, 1);
		Serial.print('\t');
		Serial.println(sum, 1);
	} else {
		Serial.print(F("Err:"));
		Serial.println(val.res, DEC);
	}
}
