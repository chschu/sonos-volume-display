/*
 * Persistent.cpp
 *
 *  Created on: 22 Oct 2016
 *      Author: chschu
 */

#include "Persistent.h"

#include <EEPROM.h>
#include <HardwareSerial.h>
#include <stddef.h>
#include <WString.h>
#include <cstring>

namespace Config {

Persistent::Persistent(uint32_t magic) :
		_magic(magic) {
}

bool Persistent::active() {
	return _data.active;
}

bool Persistent::setActive(bool active, bool dryRun) {
	if (!dryRun) {
		_data.active = active;
	}
	return true;
}

const char *Persistent::roomUUID() {
	return _data.roomUUID;
}

bool Persistent::setRoomUUID(const char *roomUUID, bool dryRun) {
	if (strlen(roomUUID) >= sizeof(_data.roomUUID)) {
		return false;
	}
	if (!dryRun) {
		strcpy(_data.roomUUID, roomUUID);
	}
	return true;
}

void Persistent::load() {
	size_t size = sizeof(_data);
	EEPROM.begin(size);
	EEPROM.get(0, _data);
	if (_data.magic == _magic) {
		Serial.println(F("magic number found, using configuration from EEPROM"));
	} else {
		Serial.println(F("magic number not found, initializing configuration in EEPROM"));
		memset(&_data, 0, size);
		_data.magic = _magic;
		_data.active = false;
		EEPROM.put(0, _data);
	}
	EEPROM.end();
}

void Persistent::save() {
	// the static_cast is just for Eclipse CDT; it doesn't accept std::size_t for the ::size_t parameter
	EEPROM.begin(static_cast<size_t>(sizeof(_data)));
	EEPROM.put(0, _data);
	EEPROM.end();
}

} /* namespace Config */
