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

#include <stddef.h>
#include <WString.h>
#include <cctype>
#include <cmath>
#include <cstdint>

#include "Color/Gradient.h"
#include "Color/RGB.h"
#include "ConfigServer.h"
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

UPnP::EventServer *eventServer = NULL;
ConfigServer *configServer = NULL;
Color::Gradient gradient;

CRGB leds[LED_COUNT];

bool active = false;
unsigned long lastWriteMillis = 0;

void showVolume(const Color::Pattern &pattern, float left, float right) {
	FastLED.clear();

	float leftLed = LED_COUNT / 2 * left;
	uint16_t leftLedInt = floor(leftLed);
	float leftLedFrac = leftLed - leftLedInt;
	for (uint16_t i = 0; i < leftLedInt; i++) {
		Color::RGB color = pattern.get(i);
		CRGB temp = { color.red, color.green, color.blue };
		leds[i] = applyGamma_video(temp, LED_GAMMA);
	}
	if (leftLedInt < LED_COUNT / 2) {
		Color::RGB color = pattern.get(leftLedInt);
		CRGB temp = { leftLedFrac * color.red, leftLedFrac * color.green, leftLedFrac * color.blue };
		leds[leftLedInt] = applyGamma_video(temp, LED_GAMMA);
	}

	float rightLed = LED_COUNT / 2 * right;
	uint16_t rightLedInt = floor(rightLed);
	float rightLedFrac = rightLed - rightLedInt;
	for (uint16_t i = 0; i < rightLedInt; i++) {
		Color::RGB color = pattern.get(LED_COUNT - 1 - i);
		CRGB temp = { color.red, color.green, color.blue };
		leds[LED_COUNT - 1 - i] = applyGamma_video(temp, LED_GAMMA);
	}
	if (rightLedInt < LED_COUNT / 2) {
		Color::RGB color = pattern.get(LED_COUNT - 1 - rightLedInt);
		CRGB temp = { rightLedFrac * color.red, rightLedFrac * color.green, rightLedFrac * color.blue };
		leds[LED_COUNT - 1 - rightLedInt] = applyGamma_video(temp, LED_GAMMA);
	}

	FastLED.show();
}

void hideVolume() {
	Serial.println("hiding");
	FastLED.clear(true);
}

void destroyEventServer() {
	if (eventServer) {
		Serial.println("destroying EventServer on WiFi disconnect");
		/* TODO unsubscribe all SIDs */
		eventServer->stop();
		delete eventServer;
		eventServer = NULL;
	}
}

void setup() {
	Serial.begin(115200);
	delay(500);

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
	gradient.set(0 * LED_COUNT / 8, green);
	gradient.set(1 * LED_COUNT / 8 - 1, yellow);
	gradient.set(1 * LED_COUNT / 8, yellow);
	gradient.set(2 * LED_COUNT / 8 - 1, red);
	gradient.set(2 * LED_COUNT / 8, red);
	gradient.set(3 * LED_COUNT / 8 - 1, magenta);
	gradient.set(3 * LED_COUNT / 8, magenta);
	gradient.set(4 * LED_COUNT / 8 - 1, white);
	gradient.set(4 * LED_COUNT / 8, white);
	gradient.set(5 * LED_COUNT / 8 - 1, magenta);
	gradient.set(5 * LED_COUNT / 8, magenta);
	gradient.set(6 * LED_COUNT / 8 - 1, red);
	gradient.set(6 * LED_COUNT / 8, red);
	gradient.set(7 * LED_COUNT / 8 - 1, yellow);
	gradient.set(7 * LED_COUNT / 8, yellow);
	gradient.set(8 * LED_COUNT / 8 - 1, green);

	// initialize LEDs
	FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_COUNT);
	FastLED.setCorrection(LED_CORRECTION);
	FastLED.setTemperature(LED_TEMPERATURE);
	FastLED.setBrightness(LED_BRIGHTNESS);

	// show startup animation
	for (int i = 0; i <= 100; i++) {
		showVolume(gradient, i / 100.0, i / 100.0);
		delay(2);
	}
	delay(498);
	for (int i = 0; i < 4; i++) {
		showVolume(gradient, 0.0, 0.0);
		delay(200);
		showVolume(gradient, 1.0, 1.0);
		delay(200);
	}
	delay(300);
	for (int i = 99; i >= 0; i--) {
		showVolume(gradient, i / 100.0, i / 100.0);
		delay(2);
	}

	// start web server for configuration
	configServer = new ConfigServer();
	configServer->onBeforeNetworkChange(destroyEventServer);
	configServer->begin();
}

void initializeEventServer() {
	Serial.println("initializing EventServer on WiFi connect");

	Serial.println(WiFi.localIP());
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.macAddress());
	eventServer = new UPnP::EventServer(WiFi.localIP());
	eventServer->begin();

	Sonos::Discover discover;
	IPAddress addr;
	if (discover.discoverAny(&addr)) {
		Serial.print(F("Found a device: "));
		Serial.println(addr);

		Sonos::ZoneGroupTopology topo(addr);
		bool discoverResult = topo.GetZoneGroupState_Decoded([](Sonos::ZoneInfo info) {
			Serial.print(F("Discovered: "));
			Serial.print(info.name);
			Serial.print(F(" @ "));
			Serial.println(info.playerIP);

			/* TODO handle subscription failure */

			String newSID;
			eventServer->subscribe([info](String SID, Stream &stream) {
						int32_t master = -1, lf = -1, rf = -1;
						XML::extractEncodedTags(stream, "</LastChange>", [&master, &lf, &rf](String tag) -> bool {

									if (!tag.startsWith("<Volume ")) {
										/* continue tag extraction */
										return true;
									}

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
										master = volume;
									} else if (channel == "LF") {
										lf = volume;
									} else if (channel == "RF") {
										rf = volume;
									}

									return true;
								});

						/* TODO keep track of current volume/mute state*/
						if (master != -1 && lf != -1 && rf != -1) {
							Serial.printf("%s: Master = %u, LF = %u, RF = %u\r\n", info.name.c_str(), master, lf, rf);
							showVolume(gradient, master * lf / 10000.0, master * rf / 10000.0);
							active = true;
							lastWriteMillis = millis();
						}

					}, "http://" + info.playerIP.toString() + ":1400/MediaRenderer/RenderingControl/Event", &newSID);

			Serial.print(F("Subscribed with new SID "));
			Serial.println(newSID);
		});
	}
}

void loop() {
	// initialize event server if WiFi is (re-)connected
	if (WiFi.isConnected() && !eventServer) {
		initializeEventServer();
	}
	// destroy event server if WiFi disconnected unexpectedly
	if (!WiFi.isConnected() && eventServer) {
		destroyEventServer();
	}
	if (eventServer) {
		eventServer->handleEvent();
	}
	configServer->handleClient();

	if (active && millis() - lastWriteMillis > 2000) {
		hideVolume();
		active = false;
	}
}
