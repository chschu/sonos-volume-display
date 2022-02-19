#ifndef UPNP_EVENTSERVER_H_
#define UPNP_EVENTSERVER_H_

#include <IPAddress.h>
#include <stddef.h>
#include <Stream.h>
#include <WiFiServer.h>
#include <WString.h>
#include <cstdint>
#include <functional>
#include <map>

namespace UPnP {

typedef std::function<void(String SID, Stream &stream)> EventCallback;

class EventServer: public WiFiServer {
public:
	EventServer(const IPAddress &addr, uint16_t callbackPort = 1400);
	EventServer(uint16_t callbackPort = 1400);

	// subscribe to an event at the endpoint defined via subscriptionURL
	// should be called after begin() to avoid missing the initial event
	// timeoutSeconds is used in the subscription request
	// a successful subscription response contains a timeout value; renewalThreshold defines the fraction of that
	// timeout after which an automatic renewal is performed in handleEvents()
	// if subscription was successful, this function returns true and stores the SID in *SID
	bool subscribe(const EventCallback &callback, const String &subscriptionURL, String *SID = nullptr, unsigned int timeoutSeconds =
			3600, double renewalThreshold = 0.9);

	// renew the subscription for the given SID
	bool renew(const String &SID);

	// unsubscribe from an event specified by its SID
	bool unsubscribe(const String &SID);

	// unsubscribe from all known events
	void unsubscribeAll();

	// handle events and subscription renewal
	void handleEvent();

private:
	struct _Subscription {
		// callback function
		EventCallback _callback;
		// URL used for subscription and renewal
		String _subscriptionURL;
		// latest renewal time of subscription, creation time if not renewed yet
		unsigned long _startMillis;
		// duration after _startMillis when renewal should be performed
		unsigned long _renewalAfterMillis;
		// timeout to be used for subsequent renewals
		unsigned int _timeoutSeconds;
		// threshold to be used for subsequent renewals
		double _renewalThreshold;
	};

	bool _renew(const String &SID, _Subscription &sub);
	bool _unsubscribe(const String &SID, _Subscription &sub);

	uint16_t _callbackPort;
	std::map<String, _Subscription> _subscriptionForSID;
};

}

#endif /* UPNP_EVENTSERVER_H_ */
