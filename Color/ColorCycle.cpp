/*
 * ColorCycle.cpp
 *
 *  Created on: 31 Oct 2016
 *      Author: chschu
 */

#include "ColorCycle.h"

#include <Arduino.h>
#include <cmath>

#include "RGB.h"

namespace Color {

ColorCycle::ColorCycle(uint16_t length, uint16_t rOffset, uint16_t gOffset, uint16_t bOffset) :
		_length(length), _colors(new RGB[length]) {
	for (int i = 0; i < length; i++) {
		uint8_t r = 127.5 * (1.0 + sin(2.0 * PI * (i + rOffset) / length));
		uint8_t g = 127.5 * (1.0 + sin(2.0 * PI * (i + gOffset) / length));
		uint8_t b = 127.5 * (1.0 + sin(2.0 * PI * (i + bOffset) / length));
		_colors[i] = {r, g, b};
	}
}

ColorCycle::~ColorCycle() {
	delete[] _colors;
}

RGB ColorCycle::get(uint16_t pos) const {
	return _colors[pos % _length];
}

} /* namespace Color */
