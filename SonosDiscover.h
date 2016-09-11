/*
 * SonosDiscover.h
 *
 *  Created on: 10 Sep 2016
 *      Author: chschu
 */

#ifndef SONOSDISCOVER_H_
#define SONOSDISCOVER_H_

class IPAddress;

class SonosDiscover {
public:
	SonosDiscover();
	~SonosDiscover();

	// discover any Sonos device on the network via UPnP/SSDP
	// returns true if a device responds within the timeout, false otherwise
	// if addr is not NULL, the device's IP address is stored in *addr
	bool discoverAny(IPAddress *addr, unsigned long timeoutMillis = 5000);
};

#endif /* SONOSDISCOVER_H_ */
