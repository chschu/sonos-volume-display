/*
 * EventServer.cpp
 *
 *  Created on: 19 Sep 2016
 *      Author: chschu
 */

#include "EventServer.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <IPAddress.h>
#include <pgmspace.h>
#include <Print.h>
#include <stddef.h>
#include <stdlib.h>
#include <WiFiClient.h>

namespace UPnP {

EventServer::EventServer(uint16_t callbackPort) :
		WiFiServer(callbackPort), _callbackPort(callbackPort) {
}

EventServer::~EventServer() {
}

bool EventServer::subscribe(EventCallback callback, String subscriptionURL, String *SID) {
	bool result = false;
	HTTPClient http;
	if (http.begin(subscriptionURL)) {
		http.addHeader("NT", "upnp:event");
		http.addHeader("Callback", "<http://" + WiFi.localIP().toString() + ":" + String(_callbackPort) + ">");
		http.addHeader("Content-Length", "0");
		// TODO add TIMEOUT header ("Second-12345")
		const char *headerKeys[] = { "SID" };
		http.collectHeaders(headerKeys, 1);
		int status = http.sendRequest("SUBSCRIBE");
		Serial.print(F("EventServer::subscribe() -> status "));
		Serial.println(status);
		if (status == 200) {
			String newSID = http.header("SID");
			if (newSID != "") {
				// check map for existing SID
				if (_subscriptionForSID.find(newSID) == _subscriptionForSID.end()) {
					// insert subscription into map
					_subscriptionForSID[newSID] = {callback, subscriptionURL};
					// return SID via parameter
					*SID = newSID;
					result = true;
				}
			}
		}
		http.end();
	}
	return result;
}

bool EventServer::unsubscribe(String SID) {
	bool result = false;
	std::map<String, _Subscription>::iterator sub = _subscriptionForSID.find(SID);
	if (sub != _subscriptionForSID.end()) {
		HTTPClient http;
		if (http.begin(sub->second._subscriptionURL)) {
			http.addHeader("SID", SID);
			http.addHeader("Content-Length", "0");
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

const char NOTIFY_RESPONSE[] PROGMEM = "HTTP/1.1 %u %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s";

static void sendResponse(WiFiClient &client, uint16_t statusCode, String statusText, String content = "",
		String contentType = "text/plain") {
	unsigned int contentLength = content.length();
	size_t size = sizeof(NOTIFY_RESPONSE) + (3 * sizeof(statusCode) - 2) + (statusText.length() - 2)
			+ (contentType.length() - 2) + (3 * sizeof(contentLength) - 2) + (contentLength - 2);
	char *buf = (char *) malloc(size);
	snprintf_P(buf, size, NOTIFY_RESPONSE, statusCode, statusText.c_str(), contentType.c_str(), contentLength,
			content.c_str());

	client.write((const char *) buf);

	free(buf);

	client.stop();
}

static void sendOK(WiFiClient &client) {
	sendResponse(client, 200, "OK");
}

static void sendBadRequest(WiFiClient &client) {
	sendResponse(client, 400, "Bad Request", "400: Bad Request");
}

static void sendPreconditionFailed(WiFiClient &client) {
	sendResponse(client, 412, "Precondition Failed", "412: Precondition Failed");
}

void EventServer::handleEvent() {
	WiFiClient client = available();
	if (client) {
		// read request line
		String requestLine = client.readStringUntil('\n');
		if (requestLine != F("NOTIFY / HTTP/1.0\r") && requestLine != F("NOTIFY / HTTP/1.1\r")) {
			Serial.println(F("invalid request line"));
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
		if (NT != "upnp:event") {
			Serial.println(F("illegal NT header value"));
			sendPreconditionFailed(client);
			return;
		}
		if (NTS != "upnp:propchange") {
			Serial.println(F("illegal NTS header value"));
			sendPreconditionFailed(client);
			return;
		}
		std::map<String, _Subscription>::iterator sub = _subscriptionForSID.find(SID);
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
}

}
