/*
 * ConfigServer.cpp
 *
 *  Created on: 30 Sep 2016
 *      Author: chschu
 */

#include "ConfigServer.h"

#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <include/wl_definitions.h>
#include <functional>

#include "JSON/Builder.h"
#include "Sonos/Discover.h"
#include "Sonos/ZoneGroupTopology.h"

ConfigServer::ConfigServer(IPAddress addr, uint16_t port) :
		_server(addr, port) {
}

ConfigServer::ConfigServer(uint16_t port) :
		_server(port) {
}

ConfigServer::~ConfigServer() {
}

void ConfigServer::begin() {
	_server.on("/api/network", HTTP_GET, std::bind(&ConfigServer::_handleGetApiNetwork, this));
	_server.on("/api/network", HTTP_POST, std::bind(&ConfigServer::_handlePostApiNetwork, this));
	_server.on("/api/discover", HTTP_GET, std::bind(&ConfigServer::_handleGetApiDiscover, this));
	_server.begin();
}

void ConfigServer::handleClient() {
	_server.handleClient();
}

void ConfigServer::stop() {
	_server.stop();
}

void ConfigServer::_handleGetApiNetwork() {
	int8_t n = WiFi.scanNetworks();

	if (n >= 0) {
		JSON::Builder json;
		json.beginArray();
		for (uint8_t i = 0; i < n; i++) {
			json.beginObject();
			json.attribute(F("ssid"), WiFi.ESP8266WiFiScanClass::SSID(i));
			json.attribute(F("bssid"), WiFi.ESP8266WiFiScanClass::BSSIDstr(i));
			json.attribute(F("rssi"), WiFi.ESP8266WiFiScanClass::RSSI(i));
			json.attribute(F("encrypted"), WiFi.encryptionType(i) != ENC_TYPE_NONE);
			json.endObject();
		}
		json.endArray();

		_server.send(200, F("application/json; charset=utf-8"), json.toString());
	} else {
		_server.send(500, F("text/plain"), F("Network Scan Failed"));
	}

	WiFi.scanDelete();
}

bool ConfigServer::needsReconnect() {
	return _reconnectSSID.length();
}

String ConfigServer::reconnectSSID() {
	return _reconnectSSID;
}

String ConfigServer::reconnectPassphrase() {
	return _reconnectPassphrase;
}

void ConfigServer::reconnectDone() {
	_reconnectSSID = _reconnectPassphrase = "";
}

void ConfigServer::_handlePostApiNetwork() {
	String plain = _server.arg(F("plain"));
	Serial.println(plain);
	String ssid = _server.arg(F("ssid"));
	String pass = _server.arg(F("pass"));
	Serial.printf("got ssid = %s, pass = %s\r\n", ssid.c_str(), pass.c_str());
	if (ssid.length() && pass.length()) {
		_reconnectSSID = ssid;
		_reconnectPassphrase = pass;
		JSON::Builder json;
		json.value(F("OK"));
		_server.send(201, F("application/json; charset=utf-8"), json.toString());
	} else {
		_server.send(400, F("text/plain"), F("Missing Request Argument"));
	}
}

void ConfigServer::_handleGetApiDiscover() {
	IPAddress addr;
	if (Sonos::Discover().discoverAny(&addr)) {
		Sonos::ZoneGroupTopology topo(addr);

		JSON::Builder json;
		json.beginArray();
		bool discoverResult = topo.GetZoneGroupState_Decoded([&json](Sonos::ZoneInfo info) {
			json.beginObject();
			json.attribute(F("uuid"), info.uuid);
			json.attribute(F("name"), info.name);
			json.attribute(F("ip"), info.playerIP.toString());
			json.endObject();
		});
		json.endArray();

		if (discoverResult) {
			_server.send(200, F("application/json; charset=utf-8"), json.toString());
		} else {
			_server.send(500, F("text/plain"), F("Error Decoding Discovery Response"));
		}
	} else {
		_server.send(404, F("text/plain"), F("No Devices Found"));
	}
}
