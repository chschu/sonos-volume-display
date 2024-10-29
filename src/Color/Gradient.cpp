#include "Gradient.h"

#include <cassert>

namespace Color {

void Gradient::set(uint16_t pos, const RGB &rgb) {
    _rgb[pos] = rgb;
}

RGB Gradient::get(uint16_t pos) const {
    // must have called "set" before
    assert(!_rgb.empty());

    // find the range for pos
    auto a = _rgb.lower_bound(pos); // smallest entry that is >= pos
    auto b = _rgb.upper_bound(pos); // smallest entry that is > pos
    if (a != b) {
        // a->first == pos -> return exact match
        return a->second;
    }
    if (a == _rgb.end()) {
        // pos > largest entry -> repeat largest entry
        return (--a)->second;
    }
    if (a == _rgb.begin()) {
        // pos < smallest entry -> repeat smallest entry
        return a->second;
    }

    // (--a)->first < pos < b->first -> interpolate
    --a;
    uint16_t pos1 = a->first;
    RGB rgb1 = a->second;
    uint16_t pos2 = b->first;
    RGB rgb2 = b->second;

    RGB result;
    result.red = rgb1.red + (pos - pos1) * (rgb2.red - rgb1.red) / (pos2 - pos1);
    result.green = rgb1.green + (pos - pos1) * (rgb2.green - rgb1.green) / (pos2 - pos1);
    result.blue = rgb1.blue + (pos - pos1) * (rgb2.blue - rgb1.blue) / (pos2 - pos1);

    return result;
}

} /* namespace Color */
