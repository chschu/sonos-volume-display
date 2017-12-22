// must be included before "max" is defined, which breaks (std::numeric_limits<...>::max)()
#include <limits>

#include "Server.h"

#include <Esp.h>
#include <ESP8266WiFi.h>
#include <HardwareSerial.h>
#include <Updater.h>

#include "../JSON/Builder.h"
#include "../Sonos/Discover.h"
#include "../Sonos/ZoneGroupTopology.h"

#include "NetworkConfig.h"
#include "SonosConfig.h"
#include "LedConfig.h"

namespace Config {

Server::Server(PersistentConfig &config, IPAddress addr, uint16_t port) :
		_config(config), _server(addr, port) {
}

Server::Server(PersistentConfig &config, uint16_t port) :
		_config(config), _server(port) {
}

void Server::begin() {
	_server.on("/api/info", HTTP_GET, std::bind(&Server::_handleGetApiInfo, this));
	_server.on("/api/discover/networks", HTTP_GET, std::bind(&Server::_handleGetApiDiscoverNetworks, this));
	_server.on("/api/discover/rooms", HTTP_GET, std::bind(&Server::_handleGetApiDiscoverRooms, this));
	_server.on("/api/config/network", HTTP_GET, std::bind(&Server::_handleGetApiConfigNetwork, this));
	_server.on("/api/config/network", HTTP_POST, std::bind(&Server::_handlePostApiConfigNetwork, this));
	_server.on("/api/config/sonos", HTTP_GET, std::bind(&Server::_handleGetApiConfigSonos, this));
	_server.on("/api/config/sonos", HTTP_POST, std::bind(&Server::_handlePostApiConfigSonos, this));
	_server.on("/api/config/led", HTTP_GET, std::bind(&Server::_handleGetApiConfigLed, this));
	_server.on("/api/config/led", HTTP_POST, std::bind(&Server::_handlePostApiConfigLed, this));
	_server.on("/api/update", HTTP_POST, std::bind(&Server::_handlePostApiUpdate, this),
			std::bind(&Server::_handlePostApiUpdateUpload, this));
	_server.begin();
}

void Server::handleClient() {
	_server.handleClient();
}

void Server::stop() {
	_server.stop();
}

void Server::onBeforeUpdate(Callback callback) {
	_beforeUpdateCallback = callback;
}

void Server::onAfterSuccessfulUpdate(Callback callback) {
	_afterSuccessfulUpdateCallback = callback;
}

void Server::onAfterFailedUpdate(Callback callback) {
	_afterFailedUpdateCallback = callback;
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

void Server::onBeforeLedConfigChange(Callback callback) {
	_beforeLedConfigChangeCallback = callback;
}

void Server::onAfterLedConfigChange(Callback callback) {
	_afterLedConfigChangeCallback = callback;
}

void Server::_handleGetApiInfo() {
	JSON::Builder json;
	json.beginObject();
	json.attribute(F("boot-mode"), ESP.getBootMode());
	json.attribute(F("boot-version"), ESP.getBootVersion());
	json.attribute(F("chip-id"), ESP.getChipId());
	json.attribute(F("core-version"), ESP.getCoreVersion());
	json.attribute(F("cpu-freq-mhz"), ESP.getCpuFreqMHz());
	json.attribute(F("cycle-count"), ESP.getCycleCount());
	json.attribute(F("flash-chip-id"), ESP.getFlashChipId());
	json.attribute(F("flash-chip-mode"), ESP.getFlashChipMode());
	json.attribute(F("flash-chip-real-size"), ESP.getFlashChipRealSize());
	json.attribute(F("flash-chip-size"), ESP.getFlashChipSize());
	json.attribute(F("flash-chip-size-by-chip-id"), ESP.getFlashChipSizeByChipId());
	json.attribute(F("flash-chip-speed"), ESP.getFlashChipSpeed());
	json.attribute(F("free-heap"), ESP.getFreeHeap());
	json.attribute(F("free-sketch-space"), ESP.getFreeSketchSpace());
	json.attribute(F("reset-info"), ESP.getResetInfo());
	json.attribute(F("reset-reason"), ESP.getResetReason());
	json.attribute(F("sdk-version"), ESP.getSdkVersion());
	json.attribute(F("sketch-md5"), ESP.getSketchMD5());
	json.attribute(F("sketch-size"), ESP.getSketchSize());
	json.endObject();
	_server.send(200, F("application/json; charset=utf-8"), json.toString());
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
	if (Sonos::Discover::any(&addr)) {
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
			&& _handleArg(F("passphrase"), networkConfig, &NetworkConfig::setPassphrase)
			&& _handleArg(F("hostname"), networkConfig, &NetworkConfig::setHostname)) {

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

void Server::_handleGetApiConfigLed() {
	_sendResponseLed(200);
}

void Server::_handlePostApiConfigLed() {
	PersistentConfig copy = _config;
	LedConfig ledConfig = copy.led();

	if (_handleArg(F("brightness"), ledConfig, &LedConfig::setBrightness)
			&& _handleArg(F("transform"), ledConfig, &LedConfig::setTransform)) {
		if (_beforeLedConfigChangeCallback) {
			_beforeLedConfigChangeCallback();
		}

		// copy modifications back and save
		_config = copy;
		_config.save();

		_sendResponseLed(200);

		if (_afterLedConfigChangeCallback) {
			_afterLedConfigChangeCallback();
		}
	} else {
		_sendResponseLed(400);
	}
}

void Server::_sendResponseNetwork(int code) {
	const NetworkConfig &networkConfig = _config.network();

	JSON::Builder json;
	json.beginObject();
	json.attribute(F("ssid"), networkConfig.ssid());
	json.attribute(F("passphrase"), F("********"));
	json.attribute(F("hostname"), networkConfig.hostname());
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

void Server::_sendResponseLed(int code) {
	const LedConfig &ledConfig = _config.led();

	JSON::Builder json;
	json.beginObject();
	json.attribute(F("brightness"), ledConfig.brightness());
	json.attribute(F("transform"), ledConfig.transform());
	json.attribute(F("status"));
	json.beginObject();
	String transformName;
	switch (ledConfig.transform()) {
	case LedConfig::Transform::IDENTITY:
		transformName = F("IDENTITY");
		break;
	case LedConfig::Transform::SQUARE:
		transformName = F("SQUARE");
		break;
	case LedConfig::Transform::SQUARE_ROOT:
		transformName = F("SQUARE_ROOT");
		break;
	case LedConfig::Transform::INVERSE_SQUARE:
		transformName = F("INVERSE_SQUARE");
		break;
	default:
		transformName = F("???");
	}
	json.attribute(F("transform-name"), transformName);
	json.endObject();
	json.endObject();

	_server.send(code, F("application/json; charset=utf-8"), json.toString());
}

void Server::_handlePostApiUpdate() {
	bool success = !Update.hasError();

	JSON::Builder json;
	json.beginObject();
	json.attribute(F("success"), success);
	json.attribute(F("md5"), Update.md5String());
	json.endObject();

	if (success) {
		_server.send(200, F("application/json; charset=utf-8"), json.toString());
		Serial.println("Rebooting...");
		ESP.restart();
	} else {
		_server.send(500, F("application/json; charset=utf-8"), json.toString());
	}
}

void Server::_handlePostApiUpdateUpload() {
	// handler for the file upload, get's the sketch bytes, and writes
	// them through the Update object
	HTTPUpload &upload = _server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		Serial.printf("Update Start: %s\r\n", upload.filename.c_str());
		uint32_t maxSketchSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
		if (Update.begin(maxSketchSize)) {
			if (_beforeUpdateCallback) {
				_beforeUpdateCallback();
			}
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (Update.write(upload.buf, upload.currentSize) == upload.currentSize) {
			Serial.printf("Update Progress: %u\r\n", upload.totalSize);
		}
		Serial.printf("Update Progress: %u\r\n", upload.totalSize);
	} else if (upload.status == UPLOAD_FILE_END) {
		if (Update.end(true)) {
			Serial.printf("Update Success: %u\r\n", upload.totalSize);
			if (_afterSuccessfulUpdateCallback) {
				_afterSuccessfulUpdateCallback();
			}
		} else {
			Serial.print("Update Failed: ");
			Update.printError(Serial);
			if (_afterFailedUpdateCallback) {
				_afterFailedUpdateCallback();
			}
		}
	} else if (upload.status == UPLOAD_FILE_ABORTED) {
		Update.end();
		Serial.println("Update Aborted");
		if (_afterFailedUpdateCallback) {
			_afterFailedUpdateCallback();
		}
	}
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

template<typename UINT_TYPE>
bool Server::_convert(const String &input, UINT_TYPE *output) {
	UINT_TYPE result = 0;
	const UINT_TYPE maxValue = (std::numeric_limits<UINT_TYPE>::max)();
	for (const char *p = input.c_str(); *p; p++) {
		if (!isdigit(*p)) {
			return false;
		}
		if (result > (maxValue - (*p - '0')) / 10) {
			return false;
		}
		result = 10 * result + (*p - '0');
	}
	*output = result;
	return true;
}

template<>
bool Server::_convert(const String &input, LedConfig::Transform *output) {
	if (input == String(LedConfig::Transform::IDENTITY)) {
		*output = LedConfig::Transform::IDENTITY;
	} else if (input == String(LedConfig::Transform::SQUARE)) {
		*output = LedConfig::Transform::SQUARE;
	} else if (input == String(LedConfig::Transform::SQUARE_ROOT)) {
		*output = LedConfig::Transform::SQUARE_ROOT;
	} else if (input == String(LedConfig::Transform::INVERSE_SQUARE)) {
		*output = LedConfig::Transform::INVERSE_SQUARE;
	} else {
		return false;
	}
	return true;
}

} /* namespace Config */
