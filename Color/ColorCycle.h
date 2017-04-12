#ifndef COLOR_COLORCYCLE_H_
#define COLOR_COLORCYCLE_H_

#include <cstdint>
#include <memory>

#include "Pattern.h"

namespace Color {

class ColorCycle: public Pattern {
public:
	ColorCycle(uint16_t length, uint16_t rOffset, uint16_t gOffset, uint16_t bOffset);

	RGB get(uint16_t pos) const;

private:
	uint16_t _length;
	std::unique_ptr<RGB[]> _colors;
};

} /* namespace Color */

#endif /* COLOR_COLORCYCLE_H_ */
