#ifndef COLOR_PATTERN_H_
#define COLOR_PATTERN_H_

#include <cstdint>

namespace Color {

struct RGB;

class Pattern {
public:
	virtual ~Pattern() {
	}

	virtual RGB get(uint16_t pos) const = 0;
};

} /* namespace Color */

#endif /* COLOR_PATTERN_H_ */
