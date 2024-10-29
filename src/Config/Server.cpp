// must be included before "max" is defined, which breaks (std::numeric_limits<...>::max)()
#include <limits>

#include "Server.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Esp.h>
#include <HardwareSerial.h>
#include <Updater.h>

#include "../Sonos/Discover.h"
#include "../Sonos/ZoneGroupTopology.h"

#include "LedConfig.h"
#include "NetworkConfig.h"
#include "SonosConfig.h"

namespace Config {

Server::Server(PersistentConfig &config, IPAddress addr, uint16_t port) : _config(config), _server(addr, port) {
}

Server::Server(PersistentConfig &config, uint16_t port) : _config(config), _server(port) {
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

void Server::onBeforeLedConfigChange(Callback callback) {
    _beforeLedConfigChangeCallback = callback;
}

void Server::onAfterLedConfigChange(Callback callback) {
    _afterLedConfigChangeCallback = callback;
}

void Server::_handleGetApiInfo() {
    JsonDocument doc;
    doc[F("boot-mode")] = ESP.getBootMode();
    doc[F("boot-version")] = ESP.getBootVersion();
    doc[F("chip-id")] = ESP.getChipId();
    doc[F("core-version")] = ESP.getCoreVersion();
    doc[F("cpu-freq-mhz")] = ESP.getCpuFreqMHz();
    doc[F("cycle-count")] = ESP.getCycleCount();
    doc[F("flash-chip-id")] = ESP.getFlashChipId();
    doc[F("flash-chip-mode")] = ESP.getFlashChipMode();
    doc[F("flash-chip-real-size")] = ESP.getFlashChipRealSize();
    doc[F("flash-chip-size")] = ESP.getFlashChipSize();
    doc[F("flash-chip-size-by-chip-id")] = ESP.getFlashChipSizeByChipId();
    doc[F("flash-chip-speed")] = ESP.getFlashChipSpeed();
    doc[F("free-heap")] = ESP.getFreeHeap();
    doc[F("free-sketch-space")] = ESP.getFreeSketchSpace();
    doc[F("reset-info")] = ESP.getResetInfo();
    doc[F("reset-reason")] = ESP.getResetReason();
    doc[F("sdk-version")] = ESP.getSdkVersion();
    doc[F("sketch-md5")] = ESP.getSketchMD5();
    doc[F("sketch-size")] = ESP.getSketchSize();

    _sendResponseJson(200, doc);
}

void Server::_handleGetApiDiscoverNetworks() {
    int8_t n = WiFi.scanNetworks();

    if (n >= 0) {
        JsonDocument doc;
        JsonArray networks = doc.to<JsonArray>();
        for (uint8_t i = 0; i < n; i++) {
            JsonObject network = networks.add<JsonObject>();
            network[F("ssid")] = WiFi.ESP8266WiFiScanClass::SSID(i);
            network[F("bssid")] = WiFi.ESP8266WiFiScanClass::BSSIDstr(i);
            network[F("rssi")] = WiFi.ESP8266WiFiScanClass::RSSI(i);
            network[F("encrypted")] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
        }

        _sendResponseJson(200, doc);
    } else {
        _server.send(500, F("text/plain"), F("Network Scan Failed"));
        _server.client().stop();
    }

    WiFi.scanDelete();
}

void Server::_handleGetApiDiscoverRooms() {
    IPAddress addr;
    if (Sonos::Discover::any(&addr)) {
        Sonos::ZoneGroupTopology topo(addr);

        JsonDocument doc;
        JsonArray rooms = doc.to<JsonArray>();
        bool discoverResult = topo.GetZoneGroupState_Decoded([rooms](Sonos::ZoneInfo info) {
            JsonObject room = rooms.add<JsonObject>();
            room[F("uuid")] = info.uuid;
            room[F("name")] = info.name;
            room[F("ip")] = info.playerIP.toString();
        });

        if (discoverResult) {
            _sendResponseJson(200, doc);
        } else {
            _server.send(500, F("text/plain"), F("Error Decoding Discovery Response"));
            _server.client().stop();
        }
    } else {
        _server.send(404, F("text/plain"), F("No Devices Found"));
        _server.client().stop();
    }
}

void Server::_handleGetApiConfigNetwork() {
    _sendResponseNetwork(200);
}

void Server::_handlePostApiConfigNetwork() {
    PersistentConfig copy = _config;
    NetworkConfig networkConfig = copy.network();

    if (_handleArg(F("ssid"), networkConfig, &NetworkConfig::setSsid) && _handleArg(F("passphrase"), networkConfig, &NetworkConfig::setPassphrase) &&
        _handleArg(F("hostname"), networkConfig, &NetworkConfig::setHostname)) {

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

    if (_handleArg(F("active"), sonosConfig, &SonosConfig::setActive) && _handleArg(F("room-uuid"), sonosConfig, &SonosConfig::setRoomUuid)) {
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

    if (_handleArg(F("brightness"), ledConfig, &LedConfig::setBrightness) && _handleArg(F("transform"), ledConfig, &LedConfig::setTransform)) {
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

    JsonDocument doc;
    doc[F("ssid")] = networkConfig.ssid();
    doc[F("passphrase")] = F("********");
    doc[F("hostname")] = networkConfig.hostname();
    JsonObject status = doc[F("status")].to<JsonObject>();
    status[F("connected")] = WiFi.isConnected();
    status[F("ssid")] = WiFi.SSID();
    status[F("passphrase")] = F("********");
    status[F("hostname")] = WiFi.hostname();
    status[F("local-ip")] = WiFi.localIP().toString();
    status[F("gateway-ip")] = WiFi.gatewayIP().toString();
    status[F("dns-ip")] = WiFi.dnsIP().toString();
    status[F("bssid")] = WiFi.BSSIDstr();
    status[F("rssi")] = WiFi.RSSI();

    _sendResponseJson(code, doc);
}

void Server::_sendResponseSonos(int code) {
    const SonosConfig &sonosConfig = _config.sonos();

    JsonDocument doc;
    doc[F("active")] = sonosConfig.active();
    doc[F("room-uuid")] = sonosConfig.roomUuid();

    _sendResponseJson(code, doc);
}

void Server::_sendResponseLed(int code) {
    const LedConfig &ledConfig = _config.led();

    JsonDocument doc;
    doc[F("brightness")] = ledConfig.brightness();
    doc[F("transform")] = ledConfig.transform();
    JsonObject choices = doc[F("choices")].to<JsonObject>();
    JsonArray transform = choices[F("transform")].to<JsonArray>();
    JsonObject identity = transform.add<JsonObject>();
    identity[F("id")] = LedConfig::Transform::IDENTITY;
    identity[F("name")] = F("IDENTITY");
    identity[F("formula")] = F("x -> x");
    JsonObject square = transform.add<JsonObject>();
    square[F("id")] = LedConfig::Transform::SQUARE;
    square[F("name")] = F("SQUARE");
    square[F("formula")] = F("x -> x * x");
    JsonObject squareRoot = transform.add<JsonObject>();
    squareRoot[F("id")] = LedConfig::Transform::SQUARE_ROOT;
    squareRoot[F("name")] = F("SQUARE_ROOT");
    squareRoot[F("formula")] = F("x -> sqrt(x)");
    JsonObject inverseSquare = transform.add<JsonObject>();
    inverseSquare[F("id")] = LedConfig::Transform::INVERSE_SQUARE;
    inverseSquare[F("name")] = F("INVERSE_SQUARE");
    inverseSquare[F("formula")] = F("x -> 1 - (1 - x) * (1 - x)");

    _sendResponseJson(code, doc);
}

void Server::_sendResponseJson(int code, JsonVariantConst source) {
    auto client = _server.client();

    client.print(F("HTTP/1.1 "));
    client.print(code);
    client.print(' ');
    client.println(_server.responseCodeToString(code));
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: "));
    client.println(measureJsonPretty(source));
    client.println(F("Connection: close"));
    client.println();
    serializeJsonPretty(source, client);

    client.stop();
}

template <typename C, typename T> bool Server::_handleArg(const String &name, C &config, bool (C::*setter)(T)) {
    if (!_server.hasArg(name)) {
        return true;
    }
    String valueString = _server.arg(name);
    T value;
    return _convert(valueString, &value) && (config.*setter)(value);
}

template <> bool Server::_convert(const String &input, bool *output) {
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

template <> bool Server::_convert(const String &input, const char **output) {
    *output = input.c_str();
    return true;
}

template <typename UINT_TYPE> bool Server::_convert(const String &input, UINT_TYPE *output) {
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

template <> bool Server::_convert(const String &input, LedConfig::Transform *output) {
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
