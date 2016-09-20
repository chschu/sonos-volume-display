#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <HardwareSerial.h>
#include <include/wl_definitions.h>
#include <IPAddress.h>
#include <WString.h>
#include <cstdint>

#include "Sonos/Discover.h"
#include "Sonos/RenderingControl.h"
#include "Sonos/ZoneGroupTopology.h"
#include "UPnP/EventServer.h"

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
		Serial.println("found a device: " + addr.toString());

		Sonos::ZoneGroupTopology topo(addr);
		bool discoverResult = topo.GetZoneGroupState_Decoded([](Sonos::ZoneInfo info) {
			Serial.print(info.uuid);
			Serial.print(F(": "));
			Serial.print(info.name);
			Serial.print(F(" @ "));
			info.playerIP.printTo(Serial);
			Serial.println();

			// TODO handle subscription failure
			String newSID;
			eventServer.subscribe([info](String SID, Stream &stream) {
						/* TODO get from stream! */
						Sonos::RenderingControl rendering(info.playerIP);
						rendering.GetVolume([](uint16_t volume) {
									Serial.print(F("Current volume: "));
									Serial.println(volume);
									double x = pow(1024.0, volume/100.0) - 1.0;
									analogWrite(LED_BUILTIN, 1023-x);
									active = true;
									lastWriteMillis = millis();
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
