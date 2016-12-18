#include "Server.h"

#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <include/wl_definitions.h>
#include <stddef.h>
#include <WString.h>

#include "../JSON/Builder.h"
#include "../Sonos/Discover.h"
#include "../Sonos/ZoneGroupTopology.h"
#include "Persistent.h"

namespace Config {

Server::Server(Persistent &config, IPAddress addr, uint16_t port) :
		_config(config), _server(addr, port) {
}

Server::Server(Persistent &config, uint16_t port) :
		_config(config), _server(port) {
}

void Server::begin() {
	_server.on("/api/discover/networks", HTTP_GET, std::bind(&Server::_handleGetApiDiscoverNetworks, this));
	_server.on("/api/discover/rooms", HTTP_GET, std::bind(&Server::_handleGetApiDiscoverRooms, this));
	_server.on("/api/network", HTTP_GET, std::bind(&Server::_handleGetApiNetwork, this));
	_server.on("/api/network", HTTP_POST, std::bind(&Server::_handlePostApiNetwork, this));
	_server.on("/api/config", HTTP_GET, std::bind(&Server::_handleGetApiConfig, this));
	_server.on("/api/config", HTTP_POST, std::bind(&Server::_handlePostApiConfig, this));
	_server.begin();
}

void Server::handleClient() {
	_server.handleClient();
}

void Server::stop() {
	_server.stop();
}

void Server::onBeforeNetworkChange(Callback callback) {
	_beforeNetworkChangeCallback = callback;
}

void Server::onBeforeConfigurationChange(Callback callback) {
	_beforeConfigurationChangeCallback = callback;
}

void Server::onAfterConfigurationChange(Callback callback) {
	_afterConfigurationChangeCallback = callback;
}

void Server::_handleGetApiDiscoverNetworks() {
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

void Server::_handleGetApiDiscoverRooms() {
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

void Server::_handleGetApiNetwork() {
	JSON::Builder json;
	json.beginObject();
	if (WiFi.isConnected()) {
		json.attribute(F("connected"), true);
		json.attribute(F("ssid"), WiFi.SSID());
		json.attribute(F("bssid"), WiFi.BSSIDstr());
		json.attribute(F("rssi"), WiFi.RSSI());
	} else {
		// funny case - where does the request come from?
		json.attribute(F("connected"), false);
	}
	json.endObject();
	_server.send(200, F("application/json; charset=utf-8"), json.toString());
}

void Server::_handlePostApiNetwork() {
	String ssid = _server.arg(F("ssid"));
	String passphrase = _server.arg(F("passphrase"));
	if (ssid.length()) {
		JSON::Builder json;
		json.value(F("OK"));
		_server.send(201, F("application/json; charset=utf-8"), json.toString());

		if (_beforeNetworkChangeCallback) {
			_beforeNetworkChangeCallback();
		}

		// reconnect with new SSID and passphrase
		Serial.println(F("WiFi disconnecting"));
		WiFi.disconnect();
		Serial.print(F("WiFi connecting with SSID "));
		Serial.println(ssid);
		WiFi.begin(ssid.c_str(), passphrase.length() ? passphrase.c_str() : NULL);

		if (_afterNetworkChangeCallback) {
			_afterNetworkChangeCallback();
		}
	} else {
		_server.send(400, F("text/plain"), F("Missing Request Argument"));
	}
}

void Server::_handleGetApiConfig() {
	JSON::Builder json;
	json.beginObject();
	json.attribute(F("active"), _config.active() ? F("true") : F("false"));
	json.attribute(F("room-uuid"), _config.roomUUID());
	json.endObject();
	_server.send(200, F("application/json; charset=utf-8"), json.toString());
}

void Server::_handlePostApiConfig() {
	bool active;
	String roomUUID;

	if (_server.hasArg(F("active"))) {
		String activeString = _server.arg(F("active"));
		if (activeString == "false") {
			active = false;
		} else if (activeString == "true") {
			active = true;
		} else {
			_server.send(400, F("text/plain"), F("Invalid Request Argument"));
			return;
		}
		if (!_config.setActive(active, true)) {
			_server.send(400, F("text/plain"), F("Invalid Request Argument"));
			return;
		}
	} else {
		active = _config.active();
	}

	if (_server.hasArg(F("room-uuid"))) {
		roomUUID = _server.arg(F("room-uuid"));
		if (!_config.setRoomUUID(roomUUID.c_str(), true)) {
			_server.send(400, F("text/plain"), F("Invalid Request Argument"));
			return;
		}
	} else {
		roomUUID = _config.roomUUID();
	}

	JSON::Builder json;
	json.value(F("OK"));
	_server.send(201, F("application/json; charset=utf-8"), json.toString());

	if (_beforeConfigurationChangeCallback) {
		_beforeConfigurationChangeCallback();
	}

	_config.setActive(active);
	_config.setRoomUUID(roomUUID.c_str());
	_config.save();

	if (_afterConfigurationChangeCallback) {
		_afterConfigurationChangeCallback();
	}
}

} /* namespace Config */
