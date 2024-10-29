#include <NeoPixelBus.h>

#include <Arduino.h>
#include <Esp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <HardwareSerial.h>
#include <IPAddress.h>
#include <pins_arduino.h>
#include <Ticker.h>
#include <ArduinoOTA.h>

#include <stddef.h>
#include <WString.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <functional>

#include "Color/ColorCycle.h"
#include "Color/Gradient.h"
#include "Color/RGB.h"
#include "Config/LedConfig.h"
#include "Config/NetworkConfig.h"
#include "Config/PersistentConfig.h"
#include "Config/Server.h"
#include "Config/SonosConfig.h"
#include "Sonos/Discover.h"
#include "Sonos/ZoneGroupTopology.h"
#include "UPnP/EventServer.h"
#include "XML/Utilities.h"

const String AP_SSID = String("svd-") + String(ESP.getChipId(), 16);

const uint16_t LED_COUNT = 24;
const uint8_t LED_PIN = D1;

Config::PersistentConfig config;
Config::Server configServer(config);
std::unique_ptr<UPnP::EventServer> eventServer;
Color::Gradient gradient;

NeoGamma<NeoGammaTableMethod> colorGamma;

NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(LED_COUNT, LED_PIN);

WiFiEventHandler sta_got_ip;
WiFiEventHandler sta_disconnected;

// color for mute
const RgbColor MUTE_COLOR(0, 0, 63);

// rainbow cycle for startup animation
const uint16_t COLOR_CYCLE_LENGTH = 57;
const Color::ColorCycle COLOR_CYCLE(COLOR_CYCLE_LENGTH, 0, COLOR_CYCLE_LENGTH / 3, 2 * COLOR_CYCLE_LENGTH / 3);

struct VolumeState {
    int8_t master = -1, lf = -1, rf = -1, mute = -1;

    bool isComplete() const {
        return master != -1 && lf != -1 && rf != -1 && mute != -1;
    }

    bool operator!=(const VolumeState &other) const {
        return master != other.master || lf != other.lf || rf != other.rf || mute != other.mute;
    }
};

class Display {
public:
    void notifyNotReady() {
        if (_state != _DS_COLOR_CYCLE) {
            // reset color cycle
            for (int i = 0; i < LED_COUNT; i++) {
                _leds[i] = RgbColor(0, 0, 0);
            }
            _colorCycleLedOffset = -1;
            _colorCycleOffset = -1;

            _state = _DS_COLOR_CYCLE;
        }
    }

    void notifyReady() {
        if (_state == _DS_COLOR_CYCLE) {
            // reset volume state
            _volumeState = VolumeState();

            _state = _DS_NOTHING;
        }
    }

    void notifyNotConnected() {
        _state = _DS_NOT_CONNECTED;
    }

    void notifyVolumeState(const VolumeState &volumeState)  {
        // check for changes
        if (volumeState.isComplete() && volumeState != _volumeState) {
            // copy changes to current state
            _volumeState = volumeState;

            Serial.printf_P(PSTR("master=%u, lf=%u, rf=%u, mute=%u\n"), _volumeState.master, _volumeState.lf, _volumeState.rf, _volumeState.mute);
            _state = _DS_VOLUME_STATE;
            if (!_volumeState.mute) {
                _volumeShownAtMillis = millis();
            }
        }
    }

    void updateDisplay(std::function<float(float)> transform) {
        if (_state == _DS_VOLUME_STATE && !_volumeState.mute && millis() > _volumeShownAtMillis + 2000) {
            _state = _DS_NOTHING;
        }

        if (_state == _DS_COLOR_CYCLE) {
            // update LEDs
            for (int i = 0; i < LED_COUNT; i++) {
                _leds[i] = RgbColor::LinearBlend(_leds[i], 0, 20.0f/255.0f);
            }
            _leds[_colorCycleLedOffset] = _toRgbColor(COLOR_CYCLE.get(_colorCycleOffset));

            // update offsets
            if (++_colorCycleLedOffset == LED_COUNT) {
                _colorCycleLedOffset = 0;
            }
            if (++_colorCycleOffset == COLOR_CYCLE_LENGTH) {
                _colorCycleOffset = 0;
            }
        } else if (_state == _DS_VOLUME_STATE) {
            for (int i = 0; i < LED_COUNT; i++) {
                _leds[i] = RgbColor(0, 0, 0);
            }

            float leftLed = LED_COUNT / 2 * transform(_volumeState.master * _volumeState.lf / 10000.0);
            uint16_t leftLedInt = floor(leftLed);
            float leftLedFrac = leftLed - leftLedInt;
            for (uint16_t i = 0; i < leftLedInt; i++) {
                _leds[i] = _volumeState.mute ? MUTE_COLOR : _toRgbColor(gradient.get(i));
            }
            if (leftLedInt < LED_COUNT / 2) {
                _leds[leftLedInt] = RgbColor::LinearBlend(
                        0,
                        _volumeState.mute ? MUTE_COLOR : _toRgbColor(gradient.get(leftLedInt)),
                                leftLedFrac);
            }

            float rightLed = LED_COUNT / 2 * transform(_volumeState.master * _volumeState.rf / 10000.0);
            uint16_t rightLedInt = floor(rightLed);
            float rightLedFrac = rightLed - rightLedInt;
            for (uint16_t i = 0; i < rightLedInt; i++) {
                _leds[LED_COUNT - 1 - i] = _volumeState.mute ? MUTE_COLOR : _toRgbColor(gradient.get(LED_COUNT - 1 - i));
            }
            if (rightLedInt < LED_COUNT / 2) {
                _leds[LED_COUNT - 1 - rightLedInt] = RgbColor::LinearBlend(
                        0,
                        _volumeState.mute ? MUTE_COLOR : _toRgbColor(gradient.get(LED_COUNT - 1 - rightLedInt)),
                                rightLedFrac);
            }
        } else if (_state == _DS_NOTHING) {
            for (int i = 0; i < LED_COUNT; i++) {
                _leds[i] = RgbColor(0, 0, 0);
            }
        } else if (_state == _DS_NOT_CONNECTED) {
            for (int i = 0; i < LED_COUNT; i++) {
                _leds[i] = RgbColor(63, 0, 0);
            }
        }

        const Config::LedConfig &ledConfig = config.led();
        for (int i = 0; i < LED_COUNT; i++) {
            strip.SetPixelColor(i, colorGamma.Correct(RgbColor::LinearBlend(0, _leds[i], ledConfig.brightness() / 255.0f)));
        }

        strip.Show();
    }

private:
    enum _DisplayState {
        _DS_COLOR_CYCLE,
        _DS_NOTHING,
        _DS_VOLUME_STATE,
        _DS_NOT_CONNECTED,
    };
    _DisplayState _state = _DS_COLOR_CYCLE;

    VolumeState _volumeState;

    unsigned long _volumeShownAtMillis;

    int16_t _colorCycleLedOffset = 0;
    int16_t _colorCycleOffset = 0;

    inline RgbColor _toRgbColor(const Color::RGB &color) {
        return RgbColor(color.red, color.green, color.blue);
    }

    RgbColor _leds[LED_COUNT];
};

Display display;

Ticker displayUpdateTicker;

bool renderingControlEventXmlTagCallback(String tag, VolumeState &volumeState) {
    if (tag.startsWith("<Volume ")) {
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
            volumeState.master = volume;
        } else if (channel == "LF") {
            volumeState.lf = volume;
        } else if (channel == "RF") {
            volumeState.rf = volume;
        }
    } else if (tag.startsWith("<Mute ")) {
        String channel;
        if (!XML::extractAttributeValue(tag, F("channel"), &channel)) {
            Serial.println(F("Failed to extract channel attribute from tag"));
            return false;
        };

        if (channel != "Master") {
            /* only Master channel is muted */
            return true;
        }

        String val;
        if (!XML::extractAttributeValue(tag, F("val"), &val)) {
            Serial.println(F("Failed to extract val attribute from tag"));
            return false;
        };

        if (val == "0") {
            volumeState.mute = 0;
        } else if (val == "1") {
            volumeState.mute = 1;
        } else {
            Serial.println(F("Invalid boolean val"));
            return false;
        }
    }

    return true;
}

void renderingControlEventCallback(String SID, Stream &stream) {
    static VolumeState volumeState;

    // update volume state
    XML::extractEncodedTags<VolumeState &>(stream, "</LastChange>", &renderingControlEventXmlTagCallback, volumeState);

    // update display
    display.notifyVolumeState(volumeState);
}

bool startWiFiStation() {
    const Config::NetworkConfig &networkConfig = config.network();
    if (networkConfig.ssid()) {
        WiFi.mode(WIFI_STA);
        WiFi.hostname(networkConfig.hostname());
        WiFi.setAutoConnect(false);
        WiFi.setAutoReconnect(false);
        WiFi.begin(networkConfig.ssid(), networkConfig.passphrase());
        return true;
    }
    return false;
}

void startWiFiAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID.c_str(), F("q1w2e3r4"));
}

void startEventServer() {
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.SSID());
    Serial.println(WiFi.macAddress());
    eventServer.reset(new UPnP::EventServer(WiFi.localIP()));
    eventServer->begin();
}

IPAddress anySonosDeviceIp;

bool findSonosDeviceIp() {
    const Config::SonosConfig &sonosConfig = config.sonos();
    if (sonosConfig.active() && Sonos::Discover::any(&anySonosDeviceIp)) {
        Serial.print(F("Found a device: "));
        Serial.println(anySonosDeviceIp);
        return true;
    }
    return false;
}

IPAddress roomSonosDeviceIp;

bool findRoomSonosDeviceIp() {
    const Config::SonosConfig &sonosConfig = config.sonos();
    if (sonosConfig.active()) {
        Sonos::ZoneGroupTopology topo(anySonosDeviceIp);
        bool found = false;
        topo.GetZoneGroupState_Decoded([sonosConfig, &found](Sonos::ZoneInfo info) {
            if (info.uuid == sonosConfig.roomUuid()) {
                Serial.print(F("Found a player with the configured room: "));
                Serial.print(info.name);
                Serial.print(F(" @ "));
                Serial.println(info.playerIP);
                roomSonosDeviceIp = info.playerIP;
                found = true;
            }
        });

        if (found) {
            return true;
        }
        Serial.println(F("Failed to find the configured room"));
    }
    return false;
}

bool subscribeToVolumeChange() {
    String newSID;

    bool result = eventServer->subscribe(renderingControlEventCallback,
            "http://" + roomSonosDeviceIp.toString() + ":1400/MediaRenderer/RenderingControl/Event", &newSID);

    if (result) {
        Serial.print(F("Subscribed with new SID "));
        Serial.println(newSID);
    } else {
        Serial.println(F("Subscription failed"));
    }

    return result;
}

void destroyEventServer() {
    eventServer.reset();
}

// transform volume value in [0,1] to display value [0,1]
float transform(float volume) {
    switch (config.led().transform()) {
    case Config::LedConfig::Transform::IDENTITY:
        return volume;
    case Config::LedConfig::Transform::SQUARE:
        return volume * volume;
    case Config::LedConfig::Transform::SQUARE_ROOT:
        return sqrt(volume);
    case Config::LedConfig::Transform::INVERSE_SQUARE:
        return volume * (2.0 - volume);
    default:
        Serial.println(F("Unknown transformation, using IDENTITY"));
        return volume;
    }
}

typedef enum {
    AS_INIT,
    AS_WIFI_NOT_CONNECTED,
    AS_WIFI_CONNECTING,
    AS_WIFI_CONNECTING_STOPPED,
    AS_WIFI_GOT_IP,
    AS_WIFI_DISCONNECTED,
    AS_EVENT_SERVER_STARTED,
    AS_ANY_SPEAKER_FOUND,
    AS_ROOM_SPEAKER_FOUND,
    AS_EVENT_SUBSCRIBED,
    AS_READY,
} ApplicationState;

ApplicationState applicationState;

void setup() {
    Serial.begin(115200);

    // prepare colors for gradient
    Color::RGB red = { 255, 0, 0 };
    Color::RGB green = { 0, 255, 0 };
    Color::RGB yellow = { 255, 255, 0 };

    // initialize color gradient for volume
    gradient.set(0 * LED_COUNT / 4, green);
    gradient.set(1 * LED_COUNT / 4 - 1, yellow);
    gradient.set(1 * LED_COUNT / 4, yellow);
    gradient.set(2 * LED_COUNT / 4 - 1, red);
    gradient.set(2 * LED_COUNT / 4, red);
    gradient.set(3 * LED_COUNT / 4 - 1, yellow);
    gradient.set(3 * LED_COUNT / 4, yellow);
    gradient.set(4 * LED_COUNT / 4 - 1, green);

    strip.Begin();

    // load configuration from EEPROM
    config.load();

    // start display update ticker
    displayUpdateTicker.attach_ms(40, []() {
        display.updateDisplay(transform);
    });

    applicationState = AS_INIT;

    // install WiFi event handlers
    sta_got_ip = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event) {
        applicationState = AS_WIFI_GOT_IP;
    });
    sta_disconnected = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event) {
        applicationState = AS_WIFI_DISCONNECTED;
    });

    // start web server for configuration
    configServer.onAfterNetworkConfigChange([]() {
        destroyEventServer();
        ESP.restart();
    });
    configServer.onAfterSonosConfigChange([]() {
        destroyEventServer();
        ESP.restart();
    });
    configServer.onAfterLedConfigChange([]() {
        destroyEventServer();
        ESP.restart();
    });
    configServer.begin();

    ArduinoOTA.begin();
}

const uint8_t INITIAL_CONNECT_RETRIES = 3;

void loop() {
    static uint8_t remainingConnectRetries = INITIAL_CONNECT_RETRIES;
    static bool allowIndefiniteWiFiReconnects = false;
    bool reconnect;

    switch (applicationState) {
    case AS_INIT:
        // notify display
        display.notifyNotReady();
        applicationState = AS_WIFI_NOT_CONNECTED;
        break;
    case AS_WIFI_NOT_CONNECTED:
        // connect/reconnect WiFi
        reconnect = false;
        if (allowIndefiniteWiFiReconnects) {
            reconnect = true;
        } else if (remainingConnectRetries) {
            remainingConnectRetries--;
            reconnect = true;
        }
        if (reconnect && startWiFiStation()) {
            Serial.println(F("Connecting to WiFi"));
            applicationState = AS_WIFI_CONNECTING;
        } else {
            Serial.println(F("No connection to WiFi - going into AP mode"));
            applicationState = AS_WIFI_CONNECTING_STOPPED;
        }
        break;
    case AS_WIFI_CONNECTING_STOPPED:
        // notify display, disable STA and enable AP for configuration
        display.notifyNotConnected();
        startWiFiAccessPoint();
        break;
    case AS_WIFI_CONNECTING:
        // nothing to do, state changed by "got ip" or "disconnected" callback
        break;
    case AS_WIFI_GOT_IP:
        // configure event server and subscription when we got an IP
        startEventServer();
        applicationState = AS_EVENT_SERVER_STARTED;
        break;
    case AS_WIFI_DISCONNECTED:
        // notify display and destroy event server
        display.notifyNotReady();
        Serial.println(F("Disconnected from WiFi"));
        destroyEventServer();
        applicationState = AS_WIFI_NOT_CONNECTED;
        break;
    case AS_EVENT_SERVER_STARTED:
        // find any Sonos device
        if (findSonosDeviceIp()) {
            applicationState = AS_ANY_SPEAKER_FOUND;
        }
        break;
    case AS_ANY_SPEAKER_FOUND:
        // find correct Sonos device
        if (findRoomSonosDeviceIp()) {
            applicationState = AS_ROOM_SPEAKER_FOUND;
        }
        break;
    case AS_ROOM_SPEAKER_FOUND:
        // subscribe to volume change
        if (subscribeToVolumeChange()) {
            applicationState = AS_EVENT_SUBSCRIBED;
        }
        break;
    case AS_EVENT_SUBSCRIBED:
        // notify the display
        display.notifyReady();
        applicationState = AS_READY;
        break;
    case AS_READY:
        // allow indefinite WiFi reconnects
        allowIndefiniteWiFiReconnects = true;
        break;
    }

    if (eventServer) {
        eventServer->handleEvent();
    }
    configServer.handleClient();

    ArduinoOTA.handle();
}
