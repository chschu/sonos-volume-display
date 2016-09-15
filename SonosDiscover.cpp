/*
 * SonosDiscover.cpp
 *
 *  Created on: 10 Sep 2016
 *      Author: chschu
 */

#include "SonosDiscover.h"

#include <IPAddress.h>
#include <Stream.h>

#include "UPnPDiscover.h"

SonosDiscover::SonosDiscover() {
}

SonosDiscover::~SonosDiscover() {
}

bool SonosDiscover::discoverAny(IPAddress *deviceIP, unsigned long timeoutMillis) {
	bool deviceFound = false;
	return UPnPDiscover().discover([&deviceFound, deviceIP](IPAddress remoteIP, Stream &stream) -> bool {
		if (!stream.find("Sonos", 5)) {
			// this is not a Sonos response; wait for the next one
			return true;
		}
		if (deviceIP) {
			*deviceIP = remoteIP;
		}
		deviceFound = true;
		// device found, stop discovery
		return false;
	}, "urn:schemas-upnp-org:device:ZonePlayer:1", 1) && deviceFound;
}
