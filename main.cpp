#include <Arduino.h>
#include <Esp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <HardwareSerial.h>
#include <include/wl_definitions.h>
#include <IPAddress.h>
#include <WString.h>
#include <cctype>
#include <cmath>
#include <cstdint>

#include "Sonos/Discover.h"
#include "Sonos/ZoneGroupTopology.h"
#include "UPnP/EventServer.h"
#include "XML/Utilities.h"

#define WIFI_SSID "..."
#define WIFI_PASS "..."

UPnP::EventServer eventServer(1400);

bool active = false;
unsigned long lastWriteMillis = 0;

void setup() {
	Serial.begin(115200);
	delay(500);

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	WiFi.mode(WIFI_STA);
	Serial.print(F("Connecting to WiFi."));
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		Serial.print('.');
	}
	Serial.println(F(" OK!"));
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.macAddress());

	eventServer.begin();

	Sonos::Discover discover;
	IPAddress addr;
	if (discover.discoverAny(&addr)) {
		Serial.print(F("Found a device: "));
		Serial.println(addr);

		Sonos::ZoneGroupTopology topo(addr);
		bool discoverResult = topo.GetZoneGroupState_Decoded([](Sonos::ZoneInfo info) {
			Serial.print(F("Discovered: "));
			Serial.print(info.name);
			Serial.print(F(" @ "));
			Serial.println(info.playerIP);

			/* TODO handle subscription failure */

			String newSID;
			eventServer.subscribe([info](String SID, Stream &stream) {
						XML::extractEncodedTags(stream, "</LastChange>", [info](String tag) -> bool {
									if (!tag.startsWith("<Volume ")) {
										/* continue tag extraction */
										return true;
									}

									String channel;
									if (!XML::extractAttributeValue(tag, F("channel"), &channel)) {
										Serial.println(F("Failed to extract channel attribute from tag"));
										return false;
									};

									String val;
									if (!XML::extractAttributeValue(tag, F("val"), &val)) {
										Serial.println(F("Failed to extract val attribute from tag"));
										return false;
									};

									uint16_t volume = 0;
									for (const char *p = val.c_str(); *p; p++) {
										if (isdigit(*p)) {
											volume = 10 * volume + (*p - '0');
										} else {
											Serial.println(F("Found a non-digit in val"));
											return false;
										}
										if (volume > 100) {
											Serial.println(F("Too large val"));
											return false;
										}
									}

									if (channel == "Master") {
										Serial.print(info.name);
										Serial.print(F(": "));
										Serial.println(volume);
										double x = pow(1024.0, volume/100.0) - 1.0;
										analogWrite(LED_BUILTIN, 1023-x);
										active = true;
										lastWriteMillis = millis();
									}

									return true;
								});

					}, "http://" + info.playerIP.toString() + ":1400/MediaRenderer/RenderingControl/Event", &newSID);

			Serial.print(F("Subscribed with new SID "));
			Serial.println(newSID);
		});
	}
	Serial.print(F("Free heap after setup: "));
	Serial.println(ESP.getFreeHeap());
}

void loop() {
	eventServer.handleEvent();
	if (active && millis() - lastWriteMillis > 2000) {
		analogWrite(LED_BUILTIN, 1023);
		active = false;
	}
}
