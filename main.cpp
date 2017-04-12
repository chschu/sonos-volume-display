#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_ESP8266_RAW_PIN_ORDER

#include <FastLED.h>
#include <color.h>
#include <colorutils.h>
#include <pixeltypes.h>

#include <Arduino.h>
#include <Esp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <HardwareSerial.h>
#include <IPAddress.h>
#include <pins_arduino.h>
#include <Ticker.h>

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
#include "Config/NetworkConfig.h"
#include "Config/PersistentConfig.h"
#include "Config/Server.h"
#include "Config/SonosConfig.h"
#include "Sonos/Discover.h"
#include "Sonos/ZoneGroupTopology.h"
#include "UPnP/EventServer.h"
#include "XML/Utilities.h"

const String HOSTNAME = String("svd-") + String(ESP.getChipId(), 16);

const uint16_t LED_COUNT = 24;
const uint8_t LED_PIN = D1;
const CRGB LED_CORRECTION = TypicalSMD5050;
const CRGB LED_TEMPERATURE = Tungsten100W;
const uint8_t LED_BRIGHTNESS = 255;
const float LED_GAMMA = 2.2;

// transformation to apply to volume values; input in [0,1], output in [0,1]
const std::function<float(float)> DISPLAY_TRANSFORMATION = [](float x) -> float {return 1.0 - pow(1.0 - x, 2.0);};

Config::PersistentConfig config;
Config::Server configServer(config);
UPnP::EventServer *eventServer = NULL;
Color::Gradient gradient;

CRGB leds[LED_COUNT];

// color for mute
const Color::RGB MUTE_COLOR = { 0, 0, 63 };

// rainbow cycle for startup animation
const uint16_t COLOR_CYCLE_LENGTH = 57;
const Color::ColorCycle COLOR_CYCLE(COLOR_CYCLE_LENGTH, 0, COLOR_CYCLE_LENGTH / 3, 2 * COLOR_CYCLE_LENGTH / 3);

// top-level states
typedef enum {
	DS_INACTIVE, DS_ACTIVE, DS_UPDATING,
} DisplayState;

// sub-states for DS_ACTIVE
typedef enum {
	DASS_HIDING, DASS_SHOWING_VOLUME, DASS_SHOWING_MUTE,
} DisplayActiveSubState;

// sub-states for DS_UPDATING
typedef enum {
	DUSS_UPDATE_IN_PROGRESS, DUSS_UPDATE_SUCCESSFUL, DUSS_UPDATE_FAILED,
} DisplayUpdatingSubState;

DisplayState displayState = DS_INACTIVE;
DisplayActiveSubState displayActiveSubState;
DisplayUpdatingSubState displayUpdatingSubState;

Ticker displayUpdateTicker;

unsigned long lastStateChangeMillis = 0;

typedef struct VolumeState {
	int8_t master, lf, rf, mute;

	void reset() {
		master = lf = rf = mute = -1;
	}

	bool complete() {
		return master != -1 && lf != -1 && rf != -1 && mute != -1;
	}

	bool operator!=(const VolumeState &other) const {
		return master != other.master || lf != other.lf || rf != other.rf || mute != other.mute;
	}
};

VolumeState current;

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
	// ignore events if not active
	if (displayState != DS_ACTIVE) {
		return;
	}

	// copy current state
	VolumeState next = current;

	// update current state
	XML::extractEncodedTags<VolumeState &>(stream, "</LastChange>", &renderingControlEventXmlTagCallback, next);

	// check for changes
	if (next != current) {
		// copy changes to current state
		current = next;

		// show if all attributes are valid
		if (current.complete()) {
			Serial.printf("master=%u, lf=%u, rf=%u, mute=%u\r\n", current.master, current.lf, current.rf, current.mute);
			displayActiveSubState = current.mute ? DASS_SHOWING_MUTE : DASS_SHOWING_VOLUME;
			lastStateChangeMillis = millis();
		}
	}
}

void subscribeToVolumeChange(Sonos::ZoneInfo &info) {
	Serial.print(F("Discovered: "));
	Serial.print(info.name);
	Serial.print(F(" @ "));
	Serial.println(info.playerIP);

	String newSID;

	// reset current state
	current.reset();

	bool result = eventServer->subscribe(renderingControlEventCallback,
			"http://" + info.playerIP.toString() + ":1400/MediaRenderer/RenderingControl/Event", &newSID);

	if (result) {
		Serial.print(F("Subscribed with new SID "));
		Serial.println(newSID);
		displayState = DS_ACTIVE;
		displayActiveSubState = DASS_HIDING;
		lastStateChangeMillis = millis();
	} else {
		Serial.println(F("Subscription failed"));
	}
}

void connectWiFi() {
	const Config::NetworkConfig &networkConfig = config.network();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(HOSTNAME.c_str(), "q1w2e3r4");
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);

	if (networkConfig.ssid() != "") {
		WiFi.enableSTA(true);
		WiFi.hostname(HOSTNAME);
		WiFi.begin(networkConfig.ssid(), networkConfig.passphrase());
	} else {
		WiFi.enableSTA(false);
		WiFi.begin();
	}
}

void initializeSubscription() {
	const Config::SonosConfig &sonosConfig = config.sonos();
	if (sonosConfig.active() && eventServer) {
		Sonos::Discover discover;
		IPAddress addr;
		if (discover.discoverAny(&addr)) {
			Serial.print(F("Found a device: "));
			Serial.println(addr);

			Sonos::ZoneGroupTopology topo(addr);
			bool discoverResult = topo.GetZoneGroupState_Decoded([&sonosConfig](Sonos::ZoneInfo info) {
				if (info.uuid == sonosConfig.roomUuid()) {
					subscribeToVolumeChange(info);
				}
			});
		}
	}
}

void destroySubscription() {
	displayState = DS_INACTIVE;
	lastStateChangeMillis = millis();

	if (eventServer) {
		eventServer->unsubscribeAll();
	}
}

void initializeEventServer() {
	Serial.println("initializing EventServer on WiFi connect");

	Serial.println(WiFi.localIP());
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.macAddress());
	eventServer = new UPnP::EventServer(WiFi.localIP());
	eventServer->begin();
}

void destroyEventServer() {
	displayState = DS_INACTIVE;
	lastStateChangeMillis = millis();

	if (eventServer) {
		Serial.println("destroying EventServer on WiFi disconnect");
		eventServer->stop();
		delete eventServer;
		eventServer = NULL;
	}
}

void updateDisplay() {
	static int16_t colorCycleOffset = -1;
	static int16_t colorCycleLedOffset = -1;
	static int16_t updateCycleLedOffset = -1;

	if (displayState == DS_ACTIVE && displayActiveSubState == DASS_SHOWING_VOLUME
			&& millis() - lastStateChangeMillis > 2000) {
		displayActiveSubState = DASS_HIDING;
		lastStateChangeMillis = millis();
	}

	if (displayState == DS_UPDATING && displayUpdatingSubState == DUSS_UPDATE_FAILED
			&& millis() - lastStateChangeMillis > 2000) {
		displayState = DS_ACTIVE;
		displayActiveSubState = DASS_HIDING;
		lastStateChangeMillis = millis();
	}

	if (displayState != DS_INACTIVE) {
		colorCycleLedOffset = -1;
	}

	if (displayState != DS_UPDATING) {
		updateCycleLedOffset = -1;
	}

	if (displayState == DS_INACTIVE) {
		if (colorCycleLedOffset < 0) {
			// initialize color cycle
			FastLED.clear();
			colorCycleLedOffset = 0;
			colorCycleOffset = 0;
		}

		// update LEDs
		for (int i = 0; i < LED_COUNT; i++) {
			leds[i] = leds[i].fadeToBlackBy(20);
		}
		Color::RGB color = COLOR_CYCLE.get(colorCycleOffset);
		leds[colorCycleLedOffset] = applyGamma_video(CRGB(color.red, color.green, color.blue), LED_GAMMA);

		// update offsets
		if (++colorCycleLedOffset == LED_COUNT) {
			colorCycleLedOffset = 0;
		}
		if (++colorCycleOffset == COLOR_CYCLE_LENGTH) {
			colorCycleOffset = 0;
		}
	} else if (displayState == DS_ACTIVE) {
		if (displayActiveSubState == DASS_SHOWING_VOLUME || displayActiveSubState == DASS_SHOWING_MUTE) {
			FastLED.clear();

			float leftLed = LED_COUNT / 2 * DISPLAY_TRANSFORMATION(current.master * current.lf / 10000.0);
			uint16_t leftLedInt = floor(leftLed);
			float leftLedFrac = leftLed - leftLedInt;
			for (uint16_t i = 0; i < leftLedInt; i++) {
				Color::RGB color = current.mute ? MUTE_COLOR : gradient.get(i);
				CRGB temp(color.red, color.green, color.blue);
				leds[i] = applyGamma_video(temp, LED_GAMMA);
			}
			if (leftLedInt < LED_COUNT / 2) {
				Color::RGB color = current.mute ? MUTE_COLOR : gradient.get(leftLedInt);
				CRGB temp(leftLedFrac * color.red, leftLedFrac * color.green, leftLedFrac * color.blue);
				leds[leftLedInt] = applyGamma_video(temp, LED_GAMMA);
			}

			float rightLed = LED_COUNT / 2 * DISPLAY_TRANSFORMATION(current.master * current.rf / 10000.0);
			uint16_t rightLedInt = floor(rightLed);
			float rightLedFrac = rightLed - rightLedInt;
			for (uint16_t i = 0; i < rightLedInt; i++) {
				Color::RGB color = current.mute ? MUTE_COLOR : gradient.get(LED_COUNT - 1 - i);
				CRGB temp(color.red, color.green, color.blue);
				leds[LED_COUNT - 1 - i] = applyGamma_video(temp, LED_GAMMA);
			}
			if (rightLedInt < LED_COUNT / 2) {
				Color::RGB color = current.mute ? MUTE_COLOR : gradient.get(LED_COUNT - 1 - rightLedInt);
				CRGB temp(rightLedFrac * color.red, rightLedFrac * color.green, rightLedFrac * color.blue);
				leds[LED_COUNT - 1 - rightLedInt] = applyGamma_video(temp, LED_GAMMA);
			}
		} else if (displayActiveSubState == DASS_HIDING) {
			FastLED.clear();
		} else {
			Serial.print(F("unknown display active sub-state: "));
			Serial.println(displayActiveSubState);
		}
	} else if (displayState == DS_UPDATING) {
		if (displayUpdatingSubState == DUSS_UPDATE_IN_PROGRESS) {
			if (updateCycleLedOffset < 0) {
				// initialize update cycle
				FastLED.clear();
				updateCycleLedOffset = 0;
			}

			// update LEDs
			for (int i = 0; i < LED_COUNT; i++) {
				leds[i] = leds[i].fadeToBlackBy(40);
			}
			leds[updateCycleLedOffset] = applyGamma_video(CRGB(255, 255, 0), LED_GAMMA);
			leds[(updateCycleLedOffset + LED_COUNT / 2) % LED_COUNT] = applyGamma_video(CRGB(255, 0, 255), LED_GAMMA);

			/* update offsets */
			if (++updateCycleLedOffset == LED_COUNT) {
				updateCycleLedOffset = 0;
			}
		} else if (displayUpdatingSubState == DUSS_UPDATE_SUCCESSFUL) {
			for (uint16_t i = 0; i < LED_COUNT; i++) {
				Color::RGB color = { 0, 255, 0 };
				CRGB temp(color.red, color.green, color.blue);
				leds[i] = applyGamma_video(temp, LED_GAMMA);
			}
		} else if (displayUpdatingSubState == DUSS_UPDATE_FAILED) {
			for (uint16_t i = 0; i < LED_COUNT; i++) {
				Color::RGB color = { 255, 0, 0 };
				CRGB temp(color.red, color.green, color.blue);
				leds[i] = applyGamma_video(temp, LED_GAMMA);
			}
		} else {
			Serial.print(F("unknown display updating sub-state: "));
			Serial.println(displayUpdatingSubState);
		}
	} else {
		Serial.print(F("unknown display state: "));
		Serial.println(displayState);
	}

	FastLED.show();
}

void setup() {
	Serial.begin(115200);

	// prepare colors for gradient
	Color::RGB red = { 255, 0, 0 };
	Color::RGB green = { 0, 255, 0 };
	Color::RGB yellow = { 255, 255, 0 };
	Color::RGB magenta = { 255, 0, 255 };
	Color::RGB white = { 255, 255, 255 };

	// initialize color gradient for volume
	gradient.set(0 * LED_COUNT / 4, green);
	gradient.set(1 * LED_COUNT / 4 - 1, yellow);
	gradient.set(1 * LED_COUNT / 4, yellow);
	gradient.set(2 * LED_COUNT / 4 - 1, red);
	gradient.set(2 * LED_COUNT / 4, red);
	gradient.set(3 * LED_COUNT / 4 - 1, yellow);
	gradient.set(3 * LED_COUNT / 4, yellow);
	gradient.set(4 * LED_COUNT / 4 - 1, green);

	// initialize LEDs
	FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_COUNT);
	FastLED.setCorrection(LED_CORRECTION);
	FastLED.setTemperature(LED_TEMPERATURE);
	FastLED.setBrightness(LED_BRIGHTNESS);

	// start display update ticker
	displayUpdateTicker.attach_ms(40, updateDisplay);

	// load configuration from EEPROM
	config.load();

	// enable WiFi and start connecting
	connectWiFi();

	// start web server for configuration
	configServer.onBeforeUpdate([]() {
		displayState = DS_UPDATING;
		displayUpdatingSubState = DUSS_UPDATE_IN_PROGRESS;
		lastStateChangeMillis = millis();
	});
	configServer.onAfterSuccessfulUpdate([]() {
		displayUpdatingSubState = DUSS_UPDATE_SUCCESSFUL;
		lastStateChangeMillis = millis();
	});
	configServer.onAfterFailedUpdate([]() {
		displayUpdatingSubState = DUSS_UPDATE_FAILED;
		lastStateChangeMillis = millis();
	});
	configServer.onBeforeNetworkConfigChange(destroyEventServer);
	configServer.onAfterNetworkConfigChange(connectWiFi);
	configServer.onBeforeSonosConfigChange(destroySubscription);
	configServer.onAfterSonosConfigChange(initializeSubscription);
	configServer.begin();
}

void loop() {
	// initialize event server if WiFi is (re-)connected
	bool connected = WiFi.isConnected();
	if (connected && !eventServer) {
		initializeEventServer();
		initializeSubscription();
	}
	// destroy event server if WiFi disconnected unexpectedly
	if (!connected && eventServer) {
		// cannot unsubscribe here, because WiFi is not connected
		destroyEventServer();
	}

	// disable AP if WiFi is connected, enable AP if WiFi is disconnected
	WiFiMode_t mode = WiFi.getMode();
	bool accessPoint = mode == WIFI_AP || mode == WIFI_AP_STA;
	if (connected == accessPoint) {
		Serial.print(!connected ? F("en") : F("dis"));
		Serial.println(F("abling AP"));
		WiFi.enableAP(!connected);
	}

	if (eventServer) {
		eventServer->handleEvent();
	}
	configServer.handleClient();
}
