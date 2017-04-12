#include "Server.h"

#include <ESP8266WiFi.h>
#include <include/wl_definitions.h>

#include "../JSON/Builder.h"
#include "../Sonos/Discover.h"
#include "../Sonos/ZoneGroupTopology.h"
#include "SonosConfig.h"
#include "NetworkConfig.h"

namespace Config {

Server::Server(PersistentConfig &config, IPAddress addr, uint16_t port) :
		_config(config), _server(addr, port) {
}

Server::Server(PersistentConfig &config, uint16_t port) :
		_config(config), _server(port) {
}

void Server::begin() {
	_server.on("/api/discover/networks", HTTP_GET, std::bind(&Server::_handleGetApiDiscoverNetworks, this));
	_server.on("/api/discover/rooms", HTTP_GET, std::bind(&Server::_handleGetApiDiscoverRooms, this));
	_server.on("/api/config/network", HTTP_GET, std::bind(&Server::_handleGetApiConfigNetwork, this));
	_server.on("/api/config/network", HTTP_POST, std::bind(&Server::_handlePostApiConfigNetwork, this));
	_server.on("/api/config/sonos", HTTP_GET, std::bind(&Server::_handleGetApiConfigSonos, this));
	_server.on("/api/config/sonos", HTTP_POST, std::bind(&Server::_handlePostApiConfigSonos, this));
	_server.begin();
}

void Server::handleClient() {
	_server.handleClient();
}

void Server::stop() {
	_server.stop();
}

void Server::onBeforeNetworkConfigChange(Callback callback) {
	_beforeNetworkConfigChangeCallback = callback;
}

void Server::onAfterNetworkConfigChange(Callback callback) {
	_afterNetworkConfigChangeCallback = callback;
}

void Server::onBeforeSonosConfigChange(Callback callback) {
	_beforeSonosConfigChangeCallback = callback;
}

void Server::onAfterSonosConfigChange(Callback callback) {
	_afterSonosConfigChangeCallback = callback;
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

void Server::_handleGetApiConfigNetwork() {
	_sendResponseNetwork(200);
}

void Server::_handlePostApiConfigNetwork() {
	PersistentConfig copy = _config;
	NetworkConfig networkConfig = copy.network();

	if (_handleArg(F("ssid"), networkConfig, &NetworkConfig::setSsid)
			&& _handleArg(F("passphrase"), networkConfig, &NetworkConfig::setPassphrase)) {

		if (_beforeNetworkConfigChangeCallback) {
			_beforeNetworkConfigChangeCallback();
		}

		// copy modifications back and save
		_config = copy;
		_config.save();

		_sendResponseNetwork(200);

		if (_afterNetworkConfigChangeCallback) {
			_afterNetworkConfigChangeCallback();
		}
	} else {
		_sendResponseNetwork(400);
	}
}

void Server::_handleGetApiConfigSonos() {
	_sendResponseSonos(200);
}

void Server::_handlePostApiConfigSonos() {
	PersistentConfig copy = _config;
	SonosConfig sonosConfig = copy.sonos();

	if (_handleArg(F("active"), sonosConfig, &SonosConfig::setActive)
			&& _handleArg(F("room-uuid"), sonosConfig, &SonosConfig::setRoomUuid)) {
		if (_beforeSonosConfigChangeCallback) {
			_beforeSonosConfigChangeCallback();
		}

		// copy modifications back and save
		_config = copy;
		_config.save();

		_sendResponseSonos(200);

		if (_afterSonosConfigChangeCallback) {
			_afterSonosConfigChangeCallback();
		}
	} else {
		_sendResponseSonos(400);
	}
}

void Server::_sendResponseNetwork(int code) {
	const NetworkConfig &networkConfig = _config.network();

	JSON::Builder json;
	json.beginObject();
	json.attribute(F("ssid"), networkConfig.ssid());
	json.attribute(F("passphrase"), F("********"));
	json.attribute(F("status"));
	json.beginObject();
	json.attribute(F("connected"), WiFi.isConnected());
	json.attribute(F("ssid"), WiFi.SSID());
	json.attribute(F("passphrase"), F("********"));
	json.attribute(F("hostname"), WiFi.hostname());
	json.attribute(F("local-ip"), WiFi.localIP().toString());
	json.attribute(F("gateway-ip"), WiFi.gatewayIP().toString());
	json.attribute(F("dns-ip"), WiFi.dnsIP().toString());
	json.attribute(F("bssid"), WiFi.BSSIDstr());
	json.attribute(F("rssi"), WiFi.RSSI());
	json.endObject();
	json.endObject();

	_server.send(code, F("application/json; charset=utf-8"), json.toString());
}
void Server::_sendResponseSonos(int code) {
	const SonosConfig &sonosConfig = _config.sonos();

	JSON::Builder json;
	json.beginObject();
	json.attribute(F("active"), sonosConfig.active());
	json.attribute(F("room-uuid"), sonosConfig.roomUuid());
	json.endObject();

	_server.send(code, F("application/json; charset=utf-8"), json.toString());
}

template<typename C, typename T>
bool Server::_handleArg(const String &name, C &config, bool (C::*setter)(T)) {
	if (!_server.hasArg(name)) {
		return true;
	}
	String valueString = _server.arg(name);
	T value;
	return _convert(valueString, &value) && (config.*setter)(value);
}

template<>
bool Server::_convert(const String &input, bool *output) {
	String lower = input;
	lower.toLowerCase();
	if (lower == "true" || lower == "yes" || lower == "t" || lower == "y" || lower == "1") {
		*output = true;
	} else if (lower == "false" || lower == "no" || lower == "f" || lower == "n" || lower == "0") {
		*output = false;
	} else {
		return false;
	}
	return true;
}

template<>
bool Server::_convert(const String &input, const char **output) {
	*output = input.c_str();
	return true;
}

} /* namespace Config */
