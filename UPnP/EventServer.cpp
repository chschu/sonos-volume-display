#include "EventServer.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <pgmspace.h>
#include <Print.h>
#include <stdlib.h>
#include <WiFiClient.h>

namespace UPnP {

EventServer::EventServer(IPAddress addr, uint16_t callbackPort) :
		WiFiServer(addr, callbackPort), _callbackPort(callbackPort) {
}

EventServer::EventServer(uint16_t callbackPort) :
		WiFiServer(callbackPort), _callbackPort(callbackPort) {
}

static unsigned int extractTimeoutSeconds(String timeoutResponseHeaderValue, unsigned int defaultValue) {
	if (timeoutResponseHeaderValue.startsWith("Second-")) {
		return atoi(timeoutResponseHeaderValue.c_str() + 7);
	}
	Serial.println(F("received TIMEOUT header without prefix; using default value"));
	return defaultValue;
}

bool EventServer::subscribe(EventCallback callback, String subscriptionURL, String *SID, unsigned int timeoutSeconds,
		double renewalThreshold) {
	bool result = false;
	HTTPClient http;
	if (http.begin(subscriptionURL)) {
		http.addHeader(F("NT"), F("upnp:event"));
		http.addHeader(F("CALLBACK"),
				String(F("<http://")) + WiFi.localIP().toString() + ':' + String(_callbackPort) + '>');
		http.addHeader(F("TIMEOUT"), String(F("Second-")) + String(timeoutSeconds));
		const char *headerKeys[] = { "SID", "TIMEOUT" };
		http.collectHeaders(headerKeys, 2);
		int status = http.sendRequest("SUBSCRIBE");
		Serial.print(F("EventServer::subscribe() -> status "));
		Serial.println(status);
		if (status == 200) {
			String newSID = http.header("SID");
			if (newSID != "") {
				// check map for existing SID
				if (_subscriptionForSID.find(newSID) == _subscriptionForSID.end()) {
					unsigned int actualTimeoutSeconds = extractTimeoutSeconds(http.header("TIMEOUT"), timeoutSeconds);
					// populate subscription
					_Subscription sub;
					sub._callback = callback;
					sub._subscriptionURL = subscriptionURL;
					sub._startMillis = millis();
					sub._renewalAfterMillis = renewalThreshold * 1000.0 * actualTimeoutSeconds;
					sub._timeoutSeconds = timeoutSeconds;
					sub._renewalThreshold = renewalThreshold;
					// insert subscription into map
					_subscriptionForSID[newSID] = sub;
					if (SID) {
						*SID = newSID;
					}
					result = true;
				}
			} else {
				Serial.println(F("missing SID header value"));
			}
		}
		http.end();
	}
	return result;
}

bool EventServer::renew(String SID) {
	auto subIt = _subscriptionForSID.find(SID);
	if (subIt == _subscriptionForSID.end()) {
		Serial.println(F("unable to renew an unknown subscription"));
		return false;
	}
	return _renew(subIt);
}

bool EventServer::_renew(std::map<String, _Subscription>::iterator subIt) {
	bool result = false;
	String SID = subIt->first;
	_Subscription &sub = subIt->second;
	Serial.print(F("renewing subscription for SID "));
	Serial.println(SID);
	HTTPClient http;
	if (http.begin(sub._subscriptionURL)) {
		http.addHeader(F("SID"), SID);
		http.addHeader(F("TIMEOUT"), String(F("Second-")) + String(sub._timeoutSeconds));
		const char *headerKeys[] = { "TIMEOUT" };
		http.collectHeaders(headerKeys, 1);
		int status = http.sendRequest("SUBSCRIBE");
		Serial.print(F("renew subscription -> status "));
		Serial.println(status);
		if (status == 200) {
			unsigned int actualTimeoutSeconds = extractTimeoutSeconds(http.header("TIMEOUT"), sub._timeoutSeconds);
			// update subscription entry
			sub._startMillis = millis();
			sub._renewalAfterMillis = sub._renewalThreshold * 1000.0 * actualTimeoutSeconds;
			result = true;
		}
		http.end();
	}
	return result;
}

bool EventServer::unsubscribe(String SID) {
	bool result = false;
	auto sub = _subscriptionForSID.find(SID);
	if (sub != _subscriptionForSID.end()) {
		HTTPClient http;
		if (http.begin(sub->second._subscriptionURL)) {
			http.addHeader(F("SID"), SID);
			int status = http.sendRequest("UNSUBSCRIBE");
			Serial.println(F("EventServer::unsubscribe() -> status "));
			Serial.println(status);
			if (status == 200) {
				_subscriptionForSID.erase(sub);
				result = true;
			}
			http.end();
		}
	}
	return result;
}

void EventServer::unsubscribeAll() {
	while (!_subscriptionForSID.empty()) {
		if (!unsubscribe(_subscriptionForSID.begin()->first)) {
			// unsubscribe failed, remove it nevertheless
			_subscriptionForSID.erase(_subscriptionForSID.begin());
		}
	}
}

const char NOTIFY_RESPONSE[] PROGMEM = "HTTP/1.1 %u %s\r\n\r\n";

static void sendResponse(WiFiClient &client, uint16_t statusCode, String statusText) {
	size_t size = sizeof(NOTIFY_RESPONSE) + (3 * sizeof(statusCode) - 2) + (statusText.length() - 2);
	char *buf = (char *) malloc(size);
	snprintf_P(buf, size, NOTIFY_RESPONSE, statusCode, statusText.c_str());

	client.write((const char *) buf);

	free(buf);

	client.stop();
}

static void sendOK(WiFiClient &client) {
	sendResponse(client, 200, F("OK"));
}

static void sendBadRequest(WiFiClient &client) {
	sendResponse(client, 400, F("Bad Request"));
}

static void sendPreconditionFailed(WiFiClient &client) {
	sendResponse(client, 412, F("Precondition Failed"));
}

void EventServer::handleEvent() {
	WiFiClient client = available();
	if (client) {
		// read request line
		String requestLine = client.readStringUntil('\n');
		if (requestLine != F("NOTIFY / HTTP/1.0\r") && requestLine != F("NOTIFY / HTTP/1.1\r")) {
			Serial.println(F("invalid request line"));
			Serial.print("\"");
			Serial.print(requestLine);
			Serial.println("\"");
			Serial.print("available: ");
			Serial.println(client.available());
			Serial.print("status: ");
			Serial.println(client.status());
			sendBadRequest(client);
			return;
		}

		// read headers
		String SID = "";
		bool NTPresent = false;
		String NT = "";
		bool NTSPresent = false;
		String NTS = "";
		while (true) {
			String headerLine = client.readStringUntil('\n');
			if (headerLine == F("\r")) {
				break;
			}
			if (!headerLine.endsWith(F("\r"))) {
				Serial.println(F("invalid header line"));
				sendBadRequest(client);
				return;
			}

			// parse header line
			int sepPos = headerLine.indexOf(':');
			if (sepPos < 0) {
				Serial.println(F("invalid header line"));
				sendBadRequest(client);
				return;
			}
			String headerName = headerLine.substring(0, sepPos);
			String headerValue = headerLine.substring(sepPos + 1);
			headerValue.trim();
			if (headerName == "SID") {
				SID = headerValue;
			} else if (headerName == "NT") {
				NT = headerValue;
				NTPresent = true;
			} else if (headerName == "NTS") {
				NTS = headerValue;
				NTSPresent = true;
			}
		}

		if (!NTPresent) {
			Serial.println(F("NT header missing"));
			sendBadRequest(client);
			return;
		}
		if (!NTSPresent) {
			Serial.println(F("NTS header missing"));
			sendBadRequest(client);
			return;
		}
		if (NT != F("upnp:event")) {
			Serial.println(F("illegal NT header value"));
			sendPreconditionFailed(client);
			return;
		}
		if (NTS != F("upnp:propchange")) {
			Serial.println(F("illegal NTS header value"));
			sendPreconditionFailed(client);
			return;
		}
		auto sub = _subscriptionForSID.find(SID);
		if (sub == _subscriptionForSID.end()) {
			Serial.println(F("unexpected SID header value"));
			sendPreconditionFailed(client);
			return;
		}

		Serial.print(F("invoking callback for SID: "));
		Serial.println(SID);
		sub->second._callback(SID, client);
		sendOK(client);
	}

	// renew all subscriptions whose _renewalAfterMillis has elapsed
	for (auto it = _subscriptionForSID.begin(); it != _subscriptionForSID.end();) {
		_Subscription &sub = it->second;
		// if renewal is required and it fails, remove the subscription
		if (millis() - sub._startMillis >= sub._renewalAfterMillis && !_renew(it)) {
			Serial.print(F("removing subscription after failed renewal for SID "));
			Serial.println(it->first);
			it = _subscriptionForSID.erase(it);
		} else {
			++it;
		}
	}
}

}
