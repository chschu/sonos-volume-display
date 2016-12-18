#include <cstring>

#include "Color/ColorCycle.h"
#include "Config/Persistent.h"
#include "Config/Server.h"

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

#include "Color/Gradient.h"
#include "Color/RGB.h"
#include "Sonos/Discover.h"
#include "Sonos/ZoneGroupTopology.h"
#include "UPnP/EventServer.h"
#include "XML/Utilities.h"

const uint16_t LED_COUNT = 24;
const uint8_t LED_PIN = D1;
const CRGB LED_CORRECTION = TypicalSMD5050;
const CRGB LED_TEMPERATURE = Tungsten100W;
const uint8_t LED_BRIGHTNESS = 255;
const float LED_GAMMA = 2.2;

Config::Persistent config;
Config::Server configServer(config);
UPnP::EventServer *eventServer = NULL;
Color::Gradient gradient;

CRGB leds[LED_COUNT];

// color for mute
const Color::RGB MUTE_COLOR = { 0, 0, 63 };

// rainbow cycle for startup animation
const uint16_t COLOR_CYCLE_LENGTH = 57;
const Color::ColorCycle COLOR_CYCLE(COLOR_CYCLE_LENGTH, 0, COLOR_CYCLE_LENGTH / 3, 2 * COLOR_CYCLE_LENGTH / 3);
uint16_t colorCycleOffset;

typedef enum {
	DS_HIDE, DS_SHOW_TEMPORARY, DS_SHOW_PERMANENT
} DisplayState;

DisplayState displayState;
unsigned long showingStartMillis = 0;
bool ready = false;
bool initialized = false;
uint16_t ledOffset;
Ticker ticker;

void setReady(bool value) {
	if (!initialized || (value != ready)) {
		if (!value) {
			FastLED.clear();
			ledOffset = 0;
			colorCycleOffset = 0;
			ticker.attach_ms(40, []() {
				for (int i = 0; i < LED_COUNT; i++) {
					leds[i] = leds[i].fadeToBlackBy(20);
				}
				Color::RGB color = COLOR_CYCLE.get(colorCycleOffset);
				leds[ledOffset] = applyGamma_video(CRGB(color.red, color.green, color.blue), LED_GAMMA);
				if (++ledOffset == LED_COUNT) {
					ledOffset = 0;
				}
				if (++colorCycleOffset == COLOR_CYCLE_LENGTH) {
					colorCycleOffset = 0;
				}
				FastLED.show();
			});
		} else {
			ticker.detach();
			FastLED.clear(true);
		}
		ready = value;
		initialized = true;
	}
}

// apply a transformation to get better resolution in the low volume range
float transformValue(float x) {
	return 1.0 - pow(1.0 - x, 2.0);
}

void showVolume(const Color::Pattern &pattern, float left, float right, bool mute) {
	if (!ready) {
		return;
	}

	Serial.print(F("showing left="));
	Serial.print(left);
	Serial.print(", right=");
	Serial.print(right);
	Serial.print(", mute=");
	Serial.println(mute);

	FastLED.clear();

	float leftLed = LED_COUNT / 2 * transformValue(left);
	uint16_t leftLedInt = floor(leftLed);
	float leftLedFrac = leftLed - leftLedInt;
	for (uint16_t i = 0; i < leftLedInt; i++) {
		Color::RGB color = mute ? MUTE_COLOR : pattern.get(i);
		CRGB temp(color.red, color.green, color.blue);
		leds[i] = applyGamma_video(temp, LED_GAMMA);
	}
	if (leftLedInt < LED_COUNT / 2) {
		Color::RGB color = mute ? MUTE_COLOR : pattern.get(leftLedInt);
		CRGB temp(leftLedFrac * color.red, leftLedFrac * color.green, leftLedFrac * color.blue);
		leds[leftLedInt] = applyGamma_video(temp, LED_GAMMA);
	}

	float rightLed = LED_COUNT / 2 * transformValue(right);
	uint16_t rightLedInt = floor(rightLed);
	float rightLedFrac = rightLed - rightLedInt;
	for (uint16_t i = 0; i < rightLedInt; i++) {
		Color::RGB color = mute ? MUTE_COLOR : pattern.get(LED_COUNT - 1 - i);
		CRGB temp(color.red, color.green, color.blue);
		leds[LED_COUNT - 1 - i] = applyGamma_video(temp, LED_GAMMA);
	}
	if (rightLedInt < LED_COUNT / 2) {
		Color::RGB color = mute ? MUTE_COLOR : pattern.get(LED_COUNT - 1 - rightLedInt);
		CRGB temp(rightLedFrac * color.red, rightLedFrac * color.green, rightLedFrac * color.blue);
		leds[LED_COUNT - 1 - rightLedInt] = applyGamma_video(temp, LED_GAMMA);
	}

	FastLED.show();
}

void hideVolume() {
	if (!ready) {
		return;
	}

	Serial.println(F("hiding"));

	FastLED.clear(true);
}

static bool changed;
static int16_t master, lf, rf;
static int8_t mute;

bool renderingControlEventXmlTagCallback(String tag) {
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
			changed |= volume != master;
			master = volume;
		} else if (channel == "LF") {
			changed |= volume != lf;
			lf = volume;
		} else if (channel == "RF") {
			changed |= volume != rf;
			rf = volume;
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
			changed |= mute != 0;
			mute = 0;
		} else if (val == "1") {
			changed |= mute != 1;
			mute = 1;
		} else {
			Serial.println(F("Invalid boolean val"));
			return false;
		}
	}

	return true;
}

void renderingControlEventCallback(String SID, Stream &stream) {
	changed = false;

	/* update current state */
	XML::extractEncodedTags(stream, "</LastChange>", &renderingControlEventXmlTagCallback);

	/* show current state */
	if (changed && master != -1 && lf != -1 && rf != -1 && mute != -1) {
		showVolume(gradient, master * lf / 10000.0, master * rf / 10000.0, mute);
		displayState = mute ? DS_SHOW_PERMANENT : DS_SHOW_TEMPORARY;
		showingStartMillis = millis();
	}
}

void subscribeToVolumeChange(Sonos::ZoneInfo &info) {
	Serial.print(F("Discovered: "));
	Serial.print(info.name);
	Serial.print(F(" @ "));
	Serial.println(info.playerIP);

	String newSID;

	master = -1, lf = -1, rf = -1, mute = -1;

	bool result = eventServer->subscribe(renderingControlEventCallback,
			"http://" + info.playerIP.toString() + ":1400/MediaRenderer/RenderingControl/Event", &newSID);

	if (result) {
		Serial.print(F("Subscribed with new SID "));
		Serial.println(newSID);
		setReady(true);
	} else {
		Serial.print(F("Subscription failed"));
	}
}

void initializeSubscription() {
	if (!config.active()) {
		setReady(true);
	} else if (eventServer) {
		Sonos::Discover discover;
		IPAddress addr;
		if (discover.discoverAny(&addr)) {
			Serial.print(F("Found a device: "));
			Serial.println(addr);

			Sonos::ZoneGroupTopology topo(addr);
			bool discoverResult = topo.GetZoneGroupState_Decoded([](Sonos::ZoneInfo info) {
				if (info.uuid == config.roomUUID()) {
					subscribeToVolumeChange(info);
				}
			});
		}
	}
}

void destroySubscription() {
	setReady(false);

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
	setReady(false);

	if (eventServer) {
		Serial.println("destroying EventServer on WiFi disconnect");
		eventServer->stop();
		delete eventServer;
		eventServer = NULL;
	}
}

void setup() {
	Serial.begin(115200);
	delay(500);

	setReady(false);

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP((String("SVD-") + String(ESP.getChipId(), 16)).c_str(), "q1w2e3r4");
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	WiFi.begin();

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

	// load configuration from EEPROM
	config.load();

	// start web server for configuration
	configServer.onBeforeNetworkChange(destroyEventServer);
	configServer.onBeforeConfigurationChange(destroySubscription);
	configServer.onAfterConfigurationChange(initializeSubscription);
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

	if (displayState == DS_SHOW_TEMPORARY && millis() - showingStartMillis > 2000) {
		hideVolume();
		displayState = DS_HIDE;
	}
}
