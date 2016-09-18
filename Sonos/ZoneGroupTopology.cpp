/*
 * ZoneGroupTopology.cpp
 *
 *  Created on: 12 Sep 2016
 *      Author: chschu
 */

#include "ZoneGroupTopology.h"

#include <ESP8266HTTPClient.h>
#include <HardwareSerial.h>
#include <pgmspace.h>
#include <stddef.h>
#include <WiFiClient.h>

namespace Sonos {

const char GET_ZONE_GROUP_STATE[] PROGMEM
		= "<?xml version=\"1.0\"?>"
				"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
				"<s:Body>"
				"<u:GetZoneGroupState xmlns:u=\"urn:schemas-upnp-org:service:ZoneGroupTopology:1\">"
				"</u:GetZoneGroupState>"
				"</s:Body>"
				"</s:Envelope>";

ZoneGroupTopology::ZoneGroupTopology(IPAddress deviceIP) :
		_deviceIP(deviceIP) {
}

ZoneGroupTopology::~ZoneGroupTopology() {
}

void replaceXmlEntities(String &s) {
	s.replace(F("&lt;"), F("<"));
	s.replace(F("&gt;"), F(">"));
	s.replace(F("&apos;"), F("'"));
	s.replace(F("&quot;"), F("\""));
	s.replace(F("&amp;"), F("&"));
	// TODO replace unicode entities with UTF-8 bytes
}

bool extractEncodedTags(Stream &stream, std::function<bool(String tag)> callback) {
	// skip everything up to (and including) the next &lt;
	while (stream.find("&lt;")) {
		String tag = "&lt;";

		// find the next &gt; that ends the encoded tag
		const char *endMarker = "&gt;";
		size_t index = 0;
		while (true) {
			char endMarkerCh = endMarker[index];
			if (!endMarkerCh) {
				// tag is complete, replace the XML entities
				replaceXmlEntities(tag);
				if (!callback(tag)) {
					Serial.println(F("Callback returned false"));
					return false;
				}
				break;
			}

			// use stream.readBytes() to do a timed read
			char ch;
			int cnt = stream.readBytes(&ch, 1);
			if (!cnt) {
				Serial.println(F("Stream ended unexpectedly"));
				return false;
			}

			// continuously match endMarker
			if (ch == endMarkerCh) {
				index++;
			} else {
				// reset after failed match
				index = 0;
			}

			// append read character
			tag += ch;
		}
	}

	return true;
}

bool ZoneGroupTopology::GetZoneGroupState_Decoded(ZoneInfoCallback callback, bool visibleOnly) {
	HTTPClient client;
	client.begin(_deviceIP.toString(), 1400, F("/ZoneGroupTopology/Control"));
	client.addHeader(F("SOAPACTION"), F("urn:schemas-upnp-org:service:ZoneGroupTopology:1#GetZoneGroupState"));
	int status = client.POST(FPSTR(GET_ZONE_GROUP_STATE));
	if (status != 200) {
		Serial.print(F("GetZoneGroupState returned HTTP status "));
		Serial.println(status);
		return false;
	}

	// the response is an XML-encoded XML string wrapped in some SOAP boilerplate
	// extract the encoded tags by matching &lt; and &gt;

	WiFiClient stream = client.getStream();

	bool result = extractEncodedTags(stream, [callback, visibleOnly](String tag) -> bool {
		if (tag.startsWith(F("<Satellite ")) || tag.startsWith(F("<ZoneGroupMember "))) {
			Serial.print(F("Found tag: "));
			Serial.println(tag);
			ZoneInfo info;

			bool visible = tag.indexOf(F(" Invisible=\"1\"")) < 0;

			if (visible || !visibleOnly) {
				int uuidStart = tag.indexOf(F(" UUID=\"")) + 7;
				if (uuidStart < 0) {
					Serial.println(F("Failed to find start of UUID in tag"));
					return false;
				}
				int uuidEnd = tag.indexOf('"', uuidStart);
				if (uuidEnd < 0) {
					Serial.println(F("Failed to find end of UUID in tag"));
					return false;
				}
				info.uuid = tag.substring(uuidStart, uuidEnd);
				replaceXmlEntities(info.uuid);

				int zoneNameStart = tag.indexOf(F(" ZoneName=\"")) + 11;
				if (zoneNameStart < 0) {
					Serial.println(F("Failed to find start of zone name in tag"));
					return false;
				}
				int zoneNameEnd = tag.indexOf('"', zoneNameStart);
				if (zoneNameEnd < 0) {
					Serial.println(F("Failed to find end of zone name in tag"));
					return false;
				}
				info.name = tag.substring(zoneNameStart, zoneNameEnd);
				replaceXmlEntities(info.name);

				int locationStart = tag.indexOf(F(" Location=\"")) + 11;
				if (locationStart < 0) {
					Serial.println(F("Failed to find start of location in tag"));
					return false;
				}
				int playerIPStart = tag.indexOf(F("//"), locationStart) + 2;
				if (playerIPStart < 0) {
					Serial.println(F("Failed to find end of player IP in tag"));
					return false;
				}
				int playerIPEnd = tag.indexOf(':', playerIPStart);
				if (playerIPEnd < 0) {
					Serial.println(F("Failed to find end of player IP in tag"));
					return false;
				}
				if (!info.playerIP.fromString(tag.substring(playerIPStart, playerIPEnd))) {
					Serial.println(F("Failed to parse player IP"));
					return false;
				}

				info.visible = visible;

				callback(info);
			}
		}

		return true;
	});

	client.end();

	return result;
}

}
