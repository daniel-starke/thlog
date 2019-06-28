/**
 * @file DHT11.hpp
 * @author Daniel Starke
 * @copyright Copyright 2019 Daniel Starke
 * @date 2019-06-01
 * @version 2019-06-10
 * 
 * DHT11 sensor library.
 */
#ifndef __DHT11_HPP__
#define __DHT11_HPP__

#include "Arduino.h"


/**
 * Class for reading a DHT11 sensor module.
 * 
 * @remarks
 * The DHT11 can provide one value per second. More frequent requests will result in an error.
 * The value from the time of the last request is always returned. Be sure to read the value
 * a second time 1100 ms later to get an up-to-date value if the last request was long ago.
 */
class DHT11 {
public:
	/** Possible error codes returned when reading the value. */
	enum ErrorCode {
		SUCCESS      = 0,
		TIMEOUT      = 1,
		NOT_READY    = 2,
		TIMING_ERROR = 3,
		PARITY_ERROR = 4
	};
	
	/** Structure with the result values. */
	struct Result {
		uint8_t res; /**< ErrorCode value */
		uint8_t rhInt;
		uint8_t rhFrac;
		uint8_t tInt;
		uint8_t tFrac;
		
		/**
		 * Constructor.
		 */
		explicit Result() {}
		
		/**
		 * Constructor.
		 * 
		 * @param[in] a0 - result value (see ErrorCode)
		 */
		explicit Result(const uint8_t a0):
			res(a0)
		{}
		
		/**
		 * Constructor.
		 * 
		 * @param[in] a0 - result value (see ErrorCode)
		 * @param[in] a1 - RH integer part
		 * @param[in] a2 - RH fraction part
		 * @param[in] a3 - temperature integer part
		 * @param[in] a4 - temperature fraction part
		 */
		explicit Result(const uint8_t a0, const uint8_t a1, const uint8_t a2, const uint8_t a3, const uint8_t a4):
			res(a0),
			rhInt(a1),
			rhFrac(a2),
			tInt(a3),
			tFrac(a4)
		{}
		
		/**
		 * Returns the relative humidity in percent.
		 * 
		 * @return RH in %
		 */
		float getRH() const {
			return static_cast<float>(this->rhInt) + (static_cast<float>(this->rhFrac) * 0.1f);
		}
		
		/**
		 * Returns the absolute temperature in degrees Celsius.
		 * 
		 * @return temperature in degrees Celsius
		 */
		float getTemp() const {
			return static_cast<float>(this->tInt) + (static_cast<float>(this->tFrac) * 0.1f);
		}
	};

	/**
	 * Initializes the given pin to read DHT11 sensor data.
	 * 
	 * @param[in] pin - pin connected to a DHT11 sensor
	 */
	static void begin(const uint8_t pin) {
		pinMode(pin, OUTPUT);
		digitalWrite(pin, HIGH);
	}

	/**
	 * Reads the last sensor value from the DHT11 sensor connected to the given pin and stores the
	 * result in the given variable.
	 * 
	 * @param[out] out - result variable to set
	 * @param[in] pin - pin to read from
	 * @param[in] impulse - impulse duration in milliseconds (optional)
	 * @return true on success, else false on error
	 * @remarks The returned values are only valid if the function returns true.
	 */
	static bool read(Result & out, const uint8_t pin, const uint8_t impulse = 25) {
		out = read(pin, impulse);
		return out.res == SUCCESS;
	}
	
	/**
	 * Reads the last sensor value from the DHT11 sensor connected to the given pin and returns the
	 * result.
	 * 
	 * @param[in] pin - pin to read from
	 * @param[in] impulse - impulse duration in milliseconds (optional)
	 * @return result
	 * @remarks The returned values are only valid if Result::res == ErrorCode::SUCCESS.
	 */
	static Result read(const uint8_t pin, const uint8_t impulse = 25) {
		/* send start impulse */
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
		delay(static_cast<unsigned long>(impulse));
		digitalWrite(pin, HIGH);
		/* wait for response */
		pinMode(pin, INPUT);
		if ( ! DHT11::wait(pin, LOW, 50) ) return Result(static_cast<uint8_t>(TIMEOUT));
		/* read response */
		delayMicroseconds(40);
		if (digitalRead(pin) != LOW) return Result(static_cast<uint8_t>(NOT_READY));
		delayMicroseconds(60);
		if (digitalRead(pin) != HIGH) return Result(static_cast<uint8_t>(NOT_READY));
		if ( ! DHT11::wait(pin, LOW, 100) ) return Result(static_cast<uint8_t>(TIMING_ERROR));
		/* read data */
		uint8_t bitMask = 0x80;
		uint8_t buf[5] = {0};
		for (uint8_t i = 0; i < 40; i++, bitMask >>= 1) {
			if ( ! DHT11::wait(pin, HIGH, 60) ) return Result(static_cast<uint8_t>(TIMING_ERROR));
			delayMicroseconds(35); /* wait longer than a 0 bit high signal */
			const bool bit = digitalRead(pin);
			if (bitMask == 0) bitMask = 0x80;
			if ( bit ) {
				buf[i >> 3] |= bitMask;
				if ( ! DHT11::wait(pin, LOW, 50) ) return Result(static_cast<uint8_t>(TIMING_ERROR));
			}
		}
		/* mask valid bits to reduce bit errors */
		buf[0] &= 0x7F; /* no negative temperature supported by DHT11 */
		buf[2] &= 0x7F;
		/* check parity */
		if (static_cast<uint8_t>(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
			return Result(static_cast<uint8_t>(PARITY_ERROR));
		}
		/* return successfully received results */
		return Result(static_cast<uint8_t>(SUCCESS), buf[0], buf[1], buf[2], buf[3]);
	}
private:
	/**
	 * Helper function which waits for the specific pin to change its state as given or times out.
	 * 
	 * @param[in] pin - read this pin
	 * @param[in] state - wait for this state
	 * @param[in] timeout - timeout in microseconds
	 * @return true if the state was reached, else false on timeout
	 */
	static bool wait(const uint8_t pin, const uint8_t state, const unsigned long timeout) {
		uint8_t lastState;
		unsigned long start = micros();
		while ((lastState = digitalRead(pin)) != state && (micros() - start) < timeout);
		return lastState == state;
	}
};


#endif /* __DHT11_HPP__ */
