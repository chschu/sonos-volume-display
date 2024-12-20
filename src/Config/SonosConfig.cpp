#include "SonosConfig.h"

#include <cstring>

namespace Config {

SonosConfig::SonosConfig(Data &data) : _data(data) {
}

bool SonosConfig::active() const {
    return _data.active;
}

bool SonosConfig::setActive(bool active) {
    _data.active = active;
    return true;
}

const char *SonosConfig::roomUuid() const {
    return _data.roomUuid;
}

bool SonosConfig::setRoomUuid(const char *roomUuid) {
    if (strlen(roomUuid) >= sizeof(_data.roomUuid)) {
        return false;
    }
    strcpy(_data.roomUuid, roomUuid);
    return true;
}

bool SonosConfig::reset() {
    return setActive(false) && setRoomUuid("");
}

bool SonosConfig::operator==(const SonosConfig &other) const {
    return _data.active == other._data.active && std::strncmp(_data.roomUuid, other._data.roomUuid, sizeof(_data.roomUuid)) == 0;
}

bool SonosConfig::operator!=(const SonosConfig &other) const {
    return !(*this == other);
}

} /* namespace Config */
