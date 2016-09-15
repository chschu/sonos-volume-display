/*
 * SonosZoneGroupTopology.h
 *
 *  Created on: 12 Sep 2016
 *      Author: chschu
 */

#ifndef SONOSZONEGROUPTOPOLOGY_H_
#define SONOSZONEGROUPTOPOLOGY_H_

#include <Arduino.h>
#include <IPAddress.h>
#include <WString.h>
#include <functional>

struct SonosZoneInfo {
	String uuid;
	String name;
	IPAddress playerIP;
	boolean visible;
};

typedef std::function<void(SonosZoneInfo info)> SonosZoneInfoCallback;

class SonosZoneGroupTopology {
public:
	SonosZoneGroupTopology(IPAddress deviceIP);
	~SonosZoneGroupTopology();

	bool GetZoneGroupState_Decoded(SonosZoneInfoCallback callback, bool visibleOnly = true);

private:
	IPAddress _deviceIP;
};

#endif /* SONOSZONEGROUPTOPOLOGY_H_ */
