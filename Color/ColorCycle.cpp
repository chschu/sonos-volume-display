#include "ColorCycle.h"

#include <Arduino.h>
#include <cmath>

namespace Color {

ColorCycle::ColorCycle(uint16_t length, uint16_t rOffset, uint16_t gOffset, uint16_t bOffset) :
		_length(length), _rOffset(rOffset), _gOffset(gOffset), _bOffset(bOffset) {
}

RGB ColorCycle::get(uint16_t pos) const {
	uint16_t i = pos % _length;
	float phi = 2.0 * PI / _length;
	return {
		127.5 * (1.0 + sinf((i + _rOffset) * phi)),
		127.5 * (1.0 + sinf((i + _gOffset) * phi)),
		127.5 * (1.0 + sinf((i + _bOffset) * phi))
	};
}

} /* namespace Color */
