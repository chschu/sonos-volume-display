#ifndef COLOR_COLORCYCLE_H_
#define COLOR_COLORCYCLE_H_

#include <cstdint>

#include "Pattern.h"
#include "RGB.h"

namespace Color {

class ColorCycle : public Pattern {
  public:
    ColorCycle(uint16_t length, uint16_t rOffset, uint16_t gOffset, uint16_t bOffset);

    RGB get(uint16_t pos) const;

  private:
    uint16_t _length;
    uint16_t _rOffset;
    uint16_t _gOffset;
    uint16_t _bOffset;
};

} /* namespace Color */

#endif /* COLOR_COLORCYCLE_H_ */
