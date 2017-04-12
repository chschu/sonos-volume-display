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

class ZoneGroupTopology {
public:
	typedef std::function<void(ZoneInfo info)> ZoneInfoCallback;

	ZoneGroupTopology(IPAddress deviceIP);

	bool GetZoneGroupState_Decoded(ZoneInfoCallback callback, bool visibleOnly = true);

private:
	IPAddress _deviceIP;
};

}

#endif /* SONOS_ZONEGROUPTOPOLOGY_H_ */
