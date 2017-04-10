#include "SonosConfig.h"
#include <cstring>

namespace Config {

SonosConfig::SonosConfig(Data &data) :
		_data(data) {
}

bool SonosConfig::active() const {
	return _data.active;
}

bool SonosConfig::setActive(bool active, bool dryRun) {
	if (!dryRun) {
		_data.active = active;
	}
	return true;
}

const char *SonosConfig::roomUUID() const {
	return _data.roomUUID;
}

bool SonosConfig::setRoomUUID(const char *roomUUID, bool dryRun) {
	if (strlen(roomUUID) >= sizeof(_data.roomUUID)) {
		return false;
	}
	if (!dryRun) {
		strcpy(_data.roomUUID, roomUUID);
	}
	return true;
}

bool SonosConfig::reset() {
	return setActive(false) && setRoomUUID("");
}

} /* namespace Config */
