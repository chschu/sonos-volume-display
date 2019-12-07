#include <NeoPixelBus.h>

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
const float LED_GAMMA = 2.2;

Config::PersistentConfig config;
Config::Server configServer(config);
UPnP::EventServer *eventServer = NULL;
Color::Gradient gradient;

NeoGamma<NeoGammaTableMethod> colorGamma;

NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(LED_COUNT, LED_PIN);

// color for mute
const RgbColor MUTE_COLOR(0, 0, 63);

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

struct VolumeState {
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

void setDisplayInactive() {
	displayState = DS_INACTIVE;
	lastStateChangeMillis = millis();
}

void setDisplayActive(DisplayActiveSubState subState) {
	displayState = DS_ACTIVE;
	displayActiveSubState = subState;
	lastStateChangeMillis = millis();
}

void setDisplayUpdating(DisplayUpdatingSubState subState) {
	displayState = DS_UPDATING;
	displayUpdatingSubState = subState;
	lastStateChangeMillis = millis();
}

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
			setDisplayActive(current.mute ? DASS_SHOWING_MUTE : DASS_SHOWING_VOLUME);
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
		setDisplayActive(DASS_HIDING);
	} else {
		Serial.println(F("Subscription failed"));
	}
}

void connectWiFi() {
	const Config::NetworkConfig &networkConfig = config.network();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(AP_SSID.c_str(), "q1w2e3r4");
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);

	if (networkConfig.ssid() != "") {
		WiFi.enableSTA(true);
		WiFi.hostname(networkConfig.hostname());
		WiFi.begin(networkConfig.ssid(), networkConfig.passphrase());
	} else {
		WiFi.enableSTA(false);
		WiFi.begin();
	}
}

void initializeSubscription() {
	const Config::SonosConfig &sonosConfig = config.sonos();
	if (sonosConfig.active() && eventServer) {
		IPAddress addr;
		if (Sonos::Discover::any(&addr)) {
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
	setDisplayInactive();

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
	setDisplayInactive();

	if (eventServer) {
		Serial.println("destroying EventServer on WiFi disconnect");
		eventServer->stop();
		delete eventServer;
		eventServer = NULL;
	}
}

void configLeds() {
	const Config::LedConfig &ledConfig = config.led();

	// show volume to visualize the brightness change
	if (displayState == DS_ACTIVE && displayActiveSubState == DASS_HIDING) {
		setDisplayActive(DASS_SHOWING_VOLUME);
	}
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
		Serial.println(F("unknown transformation, using IDENTITY"));
		return volume;
	}
}


inline RgbColor toRgbColor(const Color::RGB &color) {
	return RgbColor(color.red, color.green, color.blue);
}

void updateDisplay() {
	static int16_t colorCycleOffset = -1;
	static int16_t colorCycleLedOffset = -1;
	static int16_t updateCycleLedOffset = -1;

	static RgbColor leds[LED_COUNT];

	if (((displayState == DS_ACTIVE && displayActiveSubState == DASS_SHOWING_VOLUME)
			|| (displayState == DS_UPDATING && displayUpdatingSubState == DUSS_UPDATE_FAILED))
			&& millis() - lastStateChangeMillis > 2000) {
		setDisplayActive(DASS_HIDING);
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
			memset(leds, 0, sizeof(leds));
			colorCycleLedOffset = 0;
			colorCycleOffset = 0;
		}

		// update LEDs
		for (int i = 0; i < LED_COUNT; i++) {
			leds[i] = RgbColor::LinearBlend(leds[i], 0, 20.0/255.0);
		}
		leds[colorCycleLedOffset] = toRgbColor(COLOR_CYCLE.get(colorCycleOffset));

		// update offsets
		if (++colorCycleLedOffset == LED_COUNT) {
			colorCycleLedOffset = 0;
		}
		if (++colorCycleOffset == COLOR_CYCLE_LENGTH) {
			colorCycleOffset = 0;
		}
	} else if (displayState == DS_ACTIVE) {
		if (displayActiveSubState == DASS_SHOWING_VOLUME || displayActiveSubState == DASS_SHOWING_MUTE) {
			memset(leds, 0, sizeof(leds));

			float leftLed = LED_COUNT / 2 * transform(current.master * current.lf / 10000.0);
			uint16_t leftLedInt = floor(leftLed);
			float leftLedFrac = leftLed - leftLedInt;
			for (uint16_t i = 0; i < leftLedInt; i++) {
				leds[i] = current.mute ? MUTE_COLOR : toRgbColor(gradient.get(i));
			}
			if (leftLedInt < LED_COUNT / 2) {
				leds[leftLedInt] = RgbColor::LinearBlend(
						0,
						current.mute ? MUTE_COLOR : toRgbColor(gradient.get(leftLedInt)),
								leftLedFrac);
			}

			float rightLed = LED_COUNT / 2 * transform(current.master * current.rf / 10000.0);
			uint16_t rightLedInt = floor(rightLed);
			float rightLedFrac = rightLed - rightLedInt;
			for (uint16_t i = 0; i < rightLedInt; i++) {
				leds[LED_COUNT - 1 - i] = current.mute ? MUTE_COLOR : toRgbColor(gradient.get(LED_COUNT - 1 - i));
			}
			if (rightLedInt < LED_COUNT / 2) {
				leds[LED_COUNT - 1 - rightLedInt] = RgbColor::LinearBlend(
						0,
						current.mute ? MUTE_COLOR : toRgbColor(gradient.get(LED_COUNT - 1 - rightLedInt)),
								rightLedFrac);
			}
		} else if (displayActiveSubState == DASS_HIDING) {
			memset(leds, 0, sizeof(leds));
		} else {
			Serial.print(F("unknown display active sub-state: "));
			Serial.println(displayActiveSubState);
		}
	} else if (displayState == DS_UPDATING) {
		if (displayUpdatingSubState == DUSS_UPDATE_IN_PROGRESS) {
			if (updateCycleLedOffset < 0) {
				// initialize update cycle
				memset(leds, 0, sizeof(leds));
				updateCycleLedOffset = 0;
			}

			// update LEDs
			for (int i = 0; i < LED_COUNT; i++) {
				leds[i] = RgbColor::LinearBlend(leds[i], 0, 40.0/255.0);
			}
			leds[updateCycleLedOffset] = RgbColor(255, 255, 0);
			leds[(updateCycleLedOffset + LED_COUNT / 2) % LED_COUNT] = RgbColor(255, 0, 255);

			/* update offsets */
			if (++updateCycleLedOffset == LED_COUNT) {
				updateCycleLedOffset = 0;
			}
		} else if (displayUpdatingSubState == DUSS_UPDATE_SUCCESSFUL) {
			for (uint16_t i = 0; i < LED_COUNT; i++) {
				leds[i] = RgbColor(0, 255, 0);
			}
		} else if (displayUpdatingSubState == DUSS_UPDATE_FAILED) {
			for (uint16_t i = 0; i < LED_COUNT; i++) {
				leds[i] = RgbColor(255, 0, 0);
			}
		} else {
			Serial.print(F("unknown display updating sub-state: "));
			Serial.println(displayUpdatingSubState);
		}
	} else {
		Serial.print(F("unknown display state: "));
		Serial.println(displayState);
	}

	const Config::LedConfig &ledConfig = config.led();
	for (int i = 0; i < LED_COUNT; i++) {
		strip.SetPixelColor(i, colorGamma.Correct(RgbColor::LinearBlend(0, leds[i], ledConfig.brightness() / 255.0)));
	}

	strip.Show();
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

	strip.Begin();

	// load configuration from EEPROM
	config.load();

	// apply LED config
	configLeds();

	// start display update ticker
	displayUpdateTicker.attach_ms(40, updateDisplay);

	// enable WiFi and start connecting
	connectWiFi();

	// start web server for configuration
	configServer.onBeforeUpdate(std::bind(setDisplayUpdating, DUSS_UPDATE_IN_PROGRESS));
	configServer.onAfterSuccessfulUpdate(std::bind(setDisplayUpdating, DUSS_UPDATE_SUCCESSFUL));
	configServer.onAfterFailedUpdate(std::bind(setDisplayUpdating, DUSS_UPDATE_FAILED));
	configServer.onBeforeNetworkConfigChange(destroyEventServer);
	configServer.onAfterNetworkConfigChange(connectWiFi);
	configServer.onBeforeSonosConfigChange(destroySubscription);
	configServer.onAfterSonosConfigChange(initializeSubscription);
	configServer.onAfterLedConfigChange(configLeds);
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
