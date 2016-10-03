/*
 * ConfigServer.cpp
 *
 *  Created on: 30 Sep 2016
 *      Author: chschu
 */

#include "ConfigServer.h"

#include <ESP8266WiFi.h>
#include <include/wl_definitions.h>
#include <pgmspace.h>
#include <WString.h>
#include <functional>

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
	_server.on("/api/discover", HTTP_GET, std::bind(&ConfigServer::_handleGetApiDiscover, this));
	_server.begin();
}

void ConfigServer::handleClient() {
	_server.handleClient();
}

void ConfigServer::stop() {
	_server.stop();
}

static String jsonEscape(const String &value) {
	String escaped;
	for (const char *p = value.c_str(); *p; p++) {
		switch (*p) {
		case '"':
		case '\\':
		case '/':
			escaped += '\\';
			escaped += *p;
			break;
		case '\b':
			escaped += "\\b";
			break;
		case '\t':
			escaped += "\\t";
			break;
		case '\n':
			escaped += "\\n";
			break;
		case '\f':
			escaped += "\\f";
			break;
		case '\r':
			escaped += "\\r";
			break;
		default:
			if (*p < ' ') {
				char buf[7];
				snprintf_P(buf, sizeof(buf), PSTR("\\u%04X"), *p);
				escaped += buf;
			} else {
				escaped += *p;
			}
		}
	}
	return escaped;
}

void ConfigServer::_handleGetApiNetwork() {
	int8_t n = WiFi.scanNetworks();

	if (n >= 0) {
		String json('[');
		for (uint8_t i = 0; i < n; i++) {
			if (i > 0) {
				json += ',';
			}
			json += F("{\"ssid\":\"");
			json += WiFi.ESP8266WiFiScanClass::SSID(i);
			json += F("\",\"rssi\":");
			json += WiFi.ESP8266WiFiScanClass::RSSI(i);
			json += F(",\"encrypted\":");
			json += WiFi.encryptionType(i) != ENC_TYPE_NONE ? "true" : "false";
			json += F("}");
		}
		json += F("]");

		_server.send(200, F("application/json; charset=utf-8"), json);
	} else {
		_server.send(500, F("text/plain"), F("Network Scan Failed"));
	}

	WiFi.scanDelete();
}

void ConfigServer::_handleGetApiDiscover() {
	IPAddress addr;
	if (Sonos::Discover().discoverAny(&addr)) {
		Sonos::ZoneGroupTopology topo(addr);

		String json('[');
		bool discoverResult = topo.GetZoneGroupState_Decoded([&json](Sonos::ZoneInfo info) {
			if (json.length() > 1) {
				json += ',';
			}
			json += F("{\"uuid\":\"");
			json += jsonEscape(info.uuid);
			json += F("\",\"name\":\"");
			json += jsonEscape(info.name);
			json += F("\",\"ip\":\"");
			json += info.playerIP.toString();
			json += F("\"}");
		});
		json += F("]");

		if (discoverResult) {
			_server.send(200, F("application/json; charset=utf-8"), json);
		} else {
			_server.send(500, F("text/plain"), F("Error Decoding Discovery Response"));
		}
	} else {
		_server.send(404, F("text/plain"), F("No Devices Found"));
	}
}
