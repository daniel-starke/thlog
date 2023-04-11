/**
 * @file DHT22.hpp
 * @author Daniel Starke
 * @copyright Copyright 2019 Daniel Starke
 * @date 2019-08-07
 * @version 2023-04-11
 *
 * DHT22 sensor library.
 */
#ifndef __DHT22_HPP__
#define __DHT22_HPP__

#include "Arduino.h"


/**
 * Class for reading a DHT22 sensor module.
 *
 * @remarks
 * The DHT22 can provide one value per second. More frequent requests will result in an error.
 * The value from the time of the last request is always returned. Be sure to read the value
 * a second time 1100 ms later to get an up-to-date value if the last request was long ago.
 */
class DHT22 {
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
		uint8_t tInt;
		uint8_t tFrac;
		uint8_t rhInt;
		uint8_t rhFrac;

		/**
		 * Constructor.
		 */
		explicit Result() {}

		/**
		 * Constructor.
		 *
		 * @param[in] a0 - result value (@see ErrorCode)
		 */
		explicit Result(const uint8_t a0):
			res(a0)
		{}

		/**
		 * Constructor.
		 *
		 * @param[in] a0 - result value (see ErrorCode)
		 * @param[in] a1 - temperature integer part
		 * @param[in] a2 - temperature fraction part
		 * @param[in] a3 - RH integer part
		 * @param[in] a4 - RH fraction part
		 */
		explicit Result(const uint8_t a0, const uint8_t a1, const uint8_t a2, const uint8_t a3, const uint8_t a4):
			res(a0),
			tInt(a1),
			tFrac(a2),
			rhInt(a3),
			rhFrac(a4)
		{}

		/**
		 * Returns the absolute temperature in degrees Celsius.
		 *
		 * @return temperature in degrees Celsius
		 */
		float getTemp() const {
			if ((this->tInt & 0x80) != 0) {
				return -static_cast<float>((static_cast<unsigned int>(this->tInt & 0x7F) << 8) | this->tFrac) * 0.1f;
			}
			return static_cast<float>((static_cast<unsigned int>(this->tInt) << 8) | this->tFrac) * 0.1f;
		}

		/**
		 * Returns the relative humidity in percent.
		 *
		 * @return RH in %
		 */
		float getRH() const {
			return static_cast<float>((static_cast<unsigned int>(this->rhInt) << 8) | this->rhFrac) * 0.1f;
		}
	};

	/**
	 * Initializes the given pin to read DHT22 sensor data.
	 *
	 * @param[in] pin - pin connected to a DHT22 sensor
	 */
	static void begin(const uint8_t pin) {
		pinMode(pin, OUTPUT);
		digitalWrite(pin, HIGH);
	}

	/**
	 * Reads the last sensor value from the DHT22 sensor connected to the given pin and stores the
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
	 * Reads the last sensor value from the DHT22 sensor connected to the given pin and returns the
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
		if ( ! DHT22::wait(pin, LOW, 50) ) return Result(static_cast<uint8_t>(TIMEOUT));
		/* read response */
		delayMicroseconds(40);
		if (digitalRead(pin) != LOW) return Result(static_cast<uint8_t>(NOT_READY));
		delayMicroseconds(60);
		if (digitalRead(pin) != HIGH) return Result(static_cast<uint8_t>(NOT_READY));
		if ( ! DHT22::wait(pin, LOW, 100) ) return Result(static_cast<uint8_t>(TIMING_ERROR));
		/* read data */
		uint8_t bitMask = 0x80;
		uint8_t buf[5] = {0};
		for (uint8_t i = 0; i < 40; i++, bitMask = uint8_t(bitMask >> 1)) {
			if ( ! DHT22::wait(pin, HIGH, 80) ) return Result(static_cast<uint8_t>(TIMING_ERROR));
			delayMicroseconds(35); /* wait longer than a 0 bit high signal */
			const bool bit = digitalRead(pin);
			if (bitMask == 0) bitMask = 0x80;
			if ( bit ) {
				buf[i >> 3] |= bitMask;
				if ( ! DHT22::wait(pin, LOW, 50) ) return Result(static_cast<uint8_t>(TIMING_ERROR));
			}
		}
		/* mask valid bits to reduce bit errors */
		buf[0] &= 0x7F;
		/* check parity */
		if (static_cast<uint8_t>(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
			return Result(static_cast<uint8_t>(PARITY_ERROR));
		}
		/* return successfully received results */
		return Result(static_cast<uint8_t>(SUCCESS), buf[2], buf[3], buf[0], buf[1]);
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
		while ((lastState = uint8_t(digitalRead(pin))) != state && (micros() - start) < timeout);
		return lastState == state;
	}
};


#endif /* __DHT22_HPP__ */
