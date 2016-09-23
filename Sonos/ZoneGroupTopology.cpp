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

			// the response is an XML-encoded XML string wrapped in a <ZoneGroupState> tag and some SOAP
			// extract the encoded tags by matching &lt; and &gt;

			bool result = XML::extractEncodedTags(client.getStream(), "</ZoneGroupState>",
					[callback, visibleOnly](String tag) -> bool {
						if (!tag.startsWith(F("<Satellite ")) && !tag.startsWith(F("<ZoneGroupMember "))) {
							/* continue tag extraction */
							return true;
						}

						ZoneInfo info;
						info.visible = tag.indexOf(F(" Invisible=\"1\"")) < 0;
						if (!info.visible && visibleOnly) {
							/* continue tag extraction */
							return true;
						}

						if (!XML::extractAttributeValue(tag, F("UUID"), &info.uuid)) {
							Serial.println(F("Failed to extract UUID attribute from tag"));
							return false;
						}

						if (!XML::extractAttributeValue(tag, F("ZoneName"), &info.name)) {
							Serial.println(F("Failed to extract ZoneName attribute from tag"));
							return false;
						}

						String location;
						if (!XML::extractAttributeValue(tag, F("Location"), &location)) {
							Serial.println(F("Failed to extract Location attribute from tag"));
							return false;
						}
						int playerIPStart = location.indexOf(F("//"));
						if (playerIPStart < 0) {
							Serial.println(F("Failed to find end of player IP in Location"));
							return false;
						}
						playerIPStart += 2;
						int playerIPEnd = location.indexOf(':', playerIPStart);
						if (playerIPEnd < 0) {
							Serial.println(F("Failed to find end of player IP in Location"));
							return false;
						}
						if (!info.playerIP.fromString(location.substring(playerIPStart, playerIPEnd))) {
							Serial.println(F("Failed to parse player IP"));
							return false;
						}

						callback(info);

						return true;
					});
		}

		client.end();
	}
	return result;

}

}
