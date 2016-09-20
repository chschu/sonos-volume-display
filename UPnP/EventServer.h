/*
 * EventServer.h
 *
 *  Created on: 19 Sep 2016
 *      Author: chschu
 */

#ifndef UPNP_EVENTSERVER_H_
#define UPNP_EVENTSERVER_H_

#include <Stream.h>
#include <WiFiServer.h>
#include <WString.h>
#include <cstdint>
#include <functional>
#include <map>

namespace UPnP {

typedef std::function<void(String sid, Stream &stream)> EventCallback;

class EventServer: public WiFiServer {
public:
	EventServer(uint16_t callbackPort);
	~EventServer();

	// subscribe to an event at the endpoint defined via remoteURL
	// should be called after begin() to avoid missing the initial callback
	// if this function returns true, the subscription ID (SID) is stored in *SID
	bool subscribe(EventCallback callback, String subscriptionURL, String *SID);

	// unsubscribe from an event specified by its SID
	bool unsubscribe(String SID);

	void handleEvent();

private:
	struct _Subscription {
		EventCallback _callback;
		String _subscriptionURL;
	};

	uint16_t _callbackPort;
	std::map<String, _Subscription> _subscriptionForSID;
};

}

#endif /* UPNP_EVENTSERVER_H_ */
