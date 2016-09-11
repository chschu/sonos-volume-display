/*
 * UPnPDiscover.h
 *
 *  Created on: 10 Sep 2016
 *      Author: chschu
 */

#ifndef UPNPDISCOVER_H_
#define UPNPDISCOVER_H_

#include <IPAddress.h>
#include <Stream.h>
#include <functional>

class UPnPDiscover {
public:
	UPnPDiscover();
	~UPnPDiscover();

	// callback type
	typedef std::function<bool(IPAddress remoteIP, Stream &stream)> TCallback;

	// perform SSDP discovery using the given ST and MX values
	// for every response received within the timeout, the callback is invoked
	// if the callback returns false, discovery is terminated
	// returns false if discovery failed; not receiving any response is NOT considered a failure
	bool discover(TCallback callback, const char *st, int mx = 4,
			unsigned long timeoutMillis = 5000);
};

#endif /* UPNPDISCOVER_H_ */
