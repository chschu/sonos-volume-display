/*
 * Discover.h
 *
 *  Created on: 10 Sep 2016
 *      Author: chschu
 */

#ifndef UPNP_DISCOVER_H_
#define UPNP_DISCOVER_H_

#include <IPAddress.h>
#include <Stream.h>
#include <cstdint>
#include <functional>

namespace UPnP {

class Discover {
public:
	// discovery callback type
	typedef std::function<bool(IPAddress remoteIP, Stream &stream)> Callback;

	Discover();
	~Discover();

	// perform SSDP discovery using the given ST and MX values
	// for every response received within the timeout, the callback is invoked
	// if the callback returns false, discovery is terminated
	// returns false if discovery failed; not receiving any response is NOT considered a failure
	bool discover(Callback callback, const char *st, uint8_t mx = 4, unsigned long timeoutMillis = 5000);
};

}

#endif /* UPNP_DISCOVER_H_ */
