/*
 * Persistent.h
 *
 *  Created on: 22 Oct 2016
 *      Author: chschu
 */

#ifndef CONFIG_PERSISTENT_H_
#define CONFIG_PERSISTENT_H_

#include <cstdint>

namespace Config {

class Persistent {
public:
	Persistent(uint32_t magic = 0x51DEB00B);

	bool active();
	bool setActive(bool active, bool dryRun = false);

	const char *roomUUID();
	bool setRoomUUID(const char *roomUUID, bool dryRun = false);

	// (re-)load configuration from EEPROM
	// if magic number does not match, initialize defaults and store in EEPROM
	void load();

	// store configuration in EEPROM
	void save();

private:
	uint32_t _magic;
	struct {
		uint32_t magic;
		bool active;
		char roomUUID[32];
	} _data;
};

} /* namespace Config */

#endif /* CONFIG_PERSISTENT_H_ */
