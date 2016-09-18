/*
 * ZoneGroupTopology.h
 *
 *  Created on: 12 Sep 2016
 *      Author: chschu
 */

#ifndef SONOS_ZONEGROUPTOPOLOGY_H_
#define SONOS_ZONEGROUPTOPOLOGY_H_

#include <Arduino.h>
#include <IPAddress.h>
#include <WString.h>
#include <functional>

namespace Sonos {

struct ZoneInfo {
	String uuid;
	String name;
	IPAddress playerIP;
	boolean visible;
};

typedef std::function<void(ZoneInfo info)> ZoneInfoCallback;

class ZoneGroupTopology {
public:
	ZoneGroupTopology(IPAddress deviceIP);
	~ZoneGroupTopology();

	bool GetZoneGroupState_Decoded(ZoneInfoCallback callback, bool visibleOnly = true);

private:
	IPAddress _deviceIP;
};

}

#endif /* SONOS_ZONEGROUPTOPOLOGY_H_ */
