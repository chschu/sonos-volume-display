#ifndef COLOR_GRADIENT_H_
#define COLOR_GRADIENT_H_

#include <cstdint>
#include <map>

#include "Pattern.h"
#include "RGB.h"

namespace Color {

class Gradient : public Pattern {
  public:
    void set(uint16_t pos, const RGB &rgb);

    RGB get(uint16_t pos) const override;

  private:
    std::map<uint16_t, RGB> _rgb;
};

} /* namespace Color */

#endif /* COLOR_GRADIENT_H_ */
