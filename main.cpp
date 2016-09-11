#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <HardwareSerial.h>
#include <include/wl_definitions.h>
#include <IPAddress.h>
#include <WString.h>

#include "SonosDiscover.h"

#define WIFI_SSID "..."
#define WIFI_PASS "..."

#define SUBSCRIBE_CALLBACK_PORT 1400

HTTPClient http;
ESP8266WebServer server(SUBSCRIBE_CALLBACK_PORT);

String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

void setup() {
	Serial.begin(115200);
	delay(500);

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	WiFi.mode(WIFI_STA);
	Serial.print("Connecting to WiFi.");
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		Serial.print(".");
	}
	Serial.println(" OK!");
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.macAddress());

	SonosDiscover discover;
	IPAddress addr;
	if (discover.discoverAny(&addr)) {
		Serial.println("found a device: " + toStringIp(addr));

		server.on("/", HTTP_ANY, []() {
			digitalWrite(LED_BUILTIN, LOW);
			server.send(200);
			digitalWrite(LED_BUILTIN, HIGH);
			Serial.println("received change");
		});
		server.begin();

		http.begin(
				"http://" + toStringIp(addr)
						+ ":1400/MediaRenderer/RenderingControl/Event");
		http.addHeader("NT", "upnp:event");
		http.addHeader("Callback",
				"<http://" + toStringIp(WiFi.localIP()) + ":"
						+ SUBSCRIBE_CALLBACK_PORT + ">");
		http.addHeader("Content-Length", "0");
		const char *headerKeys[] = {"SID"};
		http.collectHeaders(headerKeys, 1);
		int result = http.sendRequest("SUBSCRIBE");
		Serial.println("result " + String(result));
		Serial.println("Subscribed with SID " + http.header("SID"));
		Serial.println(http.getString());
		http.end();
	}
}

void loop() {
	server.handleClient();
}
