#include "LedConfig.h"

#include <cstring>

namespace Config {

LedConfig::LedConfig(Data &data) : _data(data) {
}

uint8_t LedConfig::brightness() const {
    return _data.brightness;
}

bool LedConfig::setBrightness(uint8_t brightness) {
    _data.brightness = brightness;
    return true;
}

LedConfig::Transform LedConfig::transform() const {
    return _data.transform;
}

bool LedConfig::setTransform(Transform transform) {
    if (transform != Transform::IDENTITY && transform != Transform::SQUARE && transform != Transform::SQUARE_ROOT && transform != Transform::INVERSE_SQUARE) {
        return false;
    }
    _data.transform = transform;
    return true;
}

bool LedConfig::reset() {
    return setBrightness(255) && setTransform(Transform::IDENTITY);
}

bool LedConfig::operator==(const LedConfig &other) const {
    return _data.brightness == other._data.brightness && _data.transform == other._data.transform;
}

bool LedConfig::operator!=(const LedConfig &other) const {
    return !(*this == other);
}

} /* namespace Config */
