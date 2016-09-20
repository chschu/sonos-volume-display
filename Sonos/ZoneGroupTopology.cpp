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
#include <WiFiClient.h>

#include "../XML/Utilities.h"

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

bool ZoneGroupTopology::GetZoneGroupState_Decoded(ZoneInfoCallback callback, bool visibleOnly) {
	bool result = false;

	HTTPClient client;
	if (client.begin(_deviceIP.toString(), 1400, F("/ZoneGroupTopology/Control"))) {
		client.addHeader(F("SOAPACTION"), F("urn:schemas-upnp-org:service:ZoneGroupTopology:1#GetZoneGroupState"));
		int status = client.POST(FPSTR(GET_ZONE_GROUP_STATE));

		Serial.print(F("GetZoneGroupState returned HTTP status "));
		Serial.println(status);
		if (status == 200) {

			// the response is an XML-encoded XML string wrapped in some SOAP boilerplate
			// extract the encoded tags by matching &lt; and &gt;

			bool result = XML::extractEncodedTags(client.getStream(), [callback, visibleOnly](String tag) -> bool {
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
						XML::replaceEntities(info.uuid);

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
						XML::replaceEntities(info.name);

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
		}

		client.end();
	}
	return result;

}

}
