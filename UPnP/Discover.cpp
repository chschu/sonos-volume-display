#include "Discover.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <pgmspace.h>
#include <Print.h>
#include <stddef.h>
#include <stdlib.h>
#include <WiFiUdp.h>
#include <WString.h>
#include <cstdint>
#include <cstring>

namespace UPnP {

const char DISCOVER_MSEARCH[] PROGMEM = "M-SEARCH * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"MAN: \"ssdp:discover\"\r\n"
		"MX: %u\r\n"
		"ST: %s\r\n"
		"\r\n";

bool Discover::all(Callback callback, const char *st, uint8_t mx, unsigned long timeoutMillis) {
	WiFiUDP udp;

	// start UDP connection on a random port
	if (!udp.begin(0)) {
		Serial.println(F("udp.begin failed"));
		udp.stop();
		return false;
	}

	// UPnP mandates a TTL value of 4
	if (!udp.beginPacketMulticast(IPAddress(239, 255, 255, 250), 1900, WiFi.localIP(), 4)) {
		Serial.println(F("udp.beginPacketMulticast failed"));
		udp.stop();
		return false;
	}

	size_t size = sizeof(DISCOVER_MSEARCH) + (3 * sizeof(mx) - 2) + (strlen(st) - 2);
	char *buf = (char *) malloc(size);
	snprintf_P(buf, size, DISCOVER_MSEARCH, mx, st);

	// send the M-SEARCH request packet
	udp.write(buf);

	free(buf);

	if (!udp.endPacket()) {
		Serial.println(F("udp.endPacket failed"));
		udp.stop();
		return false;
	}

	unsigned long startMillis = millis();
	while (millis() - startMillis < timeoutMillis) {
		size_t packetSize = udp.parsePacket();
		if (packetSize) {
			bool keepGoing = callback(udp.remoteIP(), udp);
			udp.flush();
			if (!keepGoing) {
				break;
			}
		}
	}

	udp.stop();
	return true;
}

}
