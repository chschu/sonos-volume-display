/*
 * UPnPDiscover.cpp
 *
 *  Created on: 10 Sep 2016
 *      Author: chschu
 */

#include "UPnPDiscover.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <Print.h>
#include <stddef.h>
#include <WiFiUdp.h>
#include <WString.h>

UPnPDiscover::UPnPDiscover() {
}

UPnPDiscover::~UPnPDiscover() {
}

bool UPnPDiscover::discover(UPnPDiscover::TCallback callback, const char *st, int mx, unsigned long timeoutMillis) {
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

	// send the M-SEARCH request packet
	udp.write("M-SEARCH * HTTP/1.1\r\n");
	udp.write("HOST: 239.255.255.250:1900\r\n");
	udp.write("MAN: \"ssdp:discover\"\r\n");
	udp.write("MX: ");
	udp.write(String(mx).c_str());
	udp.write("\r\n");
	udp.write("ST: ");
	udp.write(st);
	udp.write("\r\n");
	udp.write("\r\n");
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
