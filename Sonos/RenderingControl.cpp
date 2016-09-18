/*
 * RenderingControl.cpp
 *
 *  Created on: 12 Sep 2016
 *      Author: chschu
 */

#include "RenderingControl.h"

#include <ESP8266HTTPClient.h>
#include <HardwareSerial.h>
#include <pgmspace.h>
#include <stddef.h>
#include <stdlib.h>
#include <WiFiClient.h>
#include <WString.h>
#include <cstring>

namespace Sonos {

const char GET_VOLUME[] PROGMEM
		= "<?xml version=\"1.0\"?>"
				"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
				"<s:Body>"
				"<u:GetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
				"<InstanceID>%u</InstanceID>"
				"<Channel>%s</Channel>"
				"</u:GetVolume>"
				"</s:Body>"
				"</s:Envelope>";

RenderingControl::RenderingControl(IPAddress deviceIP) :
		_deviceIP(deviceIP) {
}

RenderingControl::~RenderingControl() {
}

bool RenderingControl::GetVolume(GetVolumeCallback callback, uint32_t instanceID, const char *channel) {
	size_t size = sizeof(GET_VOLUME) - 2 + 10 - 2 + strlen(channel) + 1;
	char *buf = (char *) malloc(size);
	snprintf_P(buf, size, GET_VOLUME, instanceID, channel);

	HTTPClient client;
	client.begin(_deviceIP.toString(), 1400, F("/MediaRenderer/RenderingControl/Control"));
	client.addHeader(F("SOAPACTION"), F("urn:schemas-upnp-org:service:RenderingControl:1#GetVolume"));
	int status = client.POST((uint8_t *) buf, strlen(buf));

	free(buf);

	if (status != 200) {
		Serial.print(F("GetVolume returned HTTP status "));
		Serial.println(status);
		return false;
	}

	WiFiClient stream = client.getStream();

	bool result = false;
	if (stream.find("<CurrentVolume>")) {
		// TODO read only until next '<' and check format
		int volume = stream.parseInt();
		// must fit into an uint16_t
		if (volume >= 0 || volume <= 65535) {
			callback(volume);
		}
	} else {
		Serial.println(F("GetVolume returned an unexpected response"));
	}

	client.end();

	return result;
}

}
