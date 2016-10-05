#include <Arduino.h>
#include <Esp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <HardwareSerial.h>
#include <IPAddress.h>
#include <pins_arduino.h>
#include <stddef.h>
#include <WString.h>
#include <cctype>
#include <cmath>
#include <cstdint>

#include "ConfigServer.h"
#include "Sonos/Discover.h"
#include "Sonos/ZoneGroupTopology.h"
#include "UPnP/EventServer.h"
#include "XML/Utilities.h"

UPnP::EventServer *eventServer = NULL;
ConfigServer *configServer = NULL;

bool active = false;
unsigned long lastWriteMillis = 0;

void setup() {
	Serial.begin(115200);
	delay(500);
	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println();

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP((String("SVD-") + String(ESP.getChipId(), 16)).c_str(), "q1w2e3r4");
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	WiFi.begin();

	configServer = new ConfigServer();
	configServer->begin();
}

void initializeEventServer() {
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.macAddress());
	eventServer = new UPnP::EventServer(WiFi.localIP());
	eventServer->begin();

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
			eventServer->subscribe([info](String SID, Stream &stream) {
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
										analogWrite(D1, x);
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
}

void destroyEventServer() {
	if (eventServer) {
		// TODO unsubscribe all SIDs
		eventServer->stop();
		delete eventServer;
		eventServer = NULL;
	}
}

void loop() {
	if (eventServer) {
		eventServer->handleEvent();
	}
	configServer->handleClient();

	if (active && millis() - lastWriteMillis > 2000) {
		analogWrite(LED_BUILTIN, 1023);
		analogWrite(D1, 0);
		active = false;
	}

	if (configServer->needsReconnect()) {
		Serial.println("reconnecting with new WiFi");
		destroyEventServer();
		WiFi.disconnect();
		// TODO continue looping
		delay(1000);
		String ssid = configServer->reconnectSSID();
		String pass = configServer->reconnectPassphrase();
		WiFi.begin(ssid.c_str(), pass.c_str());
		configServer->reconnectDone();
	}

	if (WiFi.isConnected() && !eventServer) {
		Serial.println("connected with new WiFi");
		initializeEventServer();
	}
}
