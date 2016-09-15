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
#include "SonosRenderingControl.h"
#include "SonosZoneGroupTopology.h"

#define WIFI_SSID "..."
#define WIFI_PASS "..."

#define SUBSCRIBE_CALLBACK_PORT 1400

HTTPClient http;
ESP8266WebServer server(SUBSCRIBE_CALLBACK_PORT);

void setup() {
	Serial.begin(115200);
	delay(500);

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	WiFi.mode(WIFI_STA);
	Serial.print(F("Connecting to WiFi."));
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		Serial.print('.');
	}
	Serial.println(F(" OK!"));
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.macAddress());

	const char *headers[] = { "SID" };
	server.collectHeaders(headers, 1);

	server.on("/", []() {
		IPAddress clientIP = server.client().remoteIP();
		String sid = server.header("SID");

		Serial.print(F("received change from IP "));
		Serial.print(clientIP.toString());
		Serial.print(" with SID ");
		Serial.println(sid);

		digitalWrite(LED_BUILTIN, LOW);
		server.send(HTTP_CODE_OK);
		digitalWrite(LED_BUILTIN, HIGH);

		/*
		 Serial.println("Unsubscribing SID " + sid);
		 http.begin("http://" + clientIP.toString() + ":1400/MediaRenderer/RenderingControl/Event");
		 http.addHeader("HOST", clientIP.toString() + ":1400");
		 http.addHeader("SID", sid);
		 int result = http.sendRequest("UNSUBSCRIBE");
		 Serial.println("result " + String(result));
		 http.end();
		 */
	});
	server.begin();

	SonosDiscover discover;
	IPAddress addr;
	if (discover.discoverAny(&addr)) {
		Serial.println("found a device: " + addr.toString());

		SonosZoneGroupTopology topo(addr);
		bool discoverResult = topo.GetZoneGroupState_Decoded([](SonosZoneInfo info) {
			Serial.print(info.uuid);
			Serial.print(F(": "));
			Serial.print(info.name);
			Serial.print(F(" @ "));
			info.playerIP.printTo(Serial);
			Serial.println();

			SonosRenderingControl rendering(info.playerIP);
			rendering.GetVolume([](uint16_t volume) {
						Serial.print(F("Current volume: "));
						Serial.println(volume);
					});

			http.begin("http://" + info.playerIP.toString() + ":1400/MediaRenderer/RenderingControl/Event");
			http.addHeader("NT", "upnp:event");
			http.addHeader("Callback", "<http://" + WiFi.localIP().toString() + ":" + SUBSCRIBE_CALLBACK_PORT + ">");
			http.addHeader("Content-Length", "0");
			const char *headerKeys[] = {"SID"};
			http.collectHeaders(headerKeys, 1);
			int result = http.sendRequest("SUBSCRIBE");
			Serial.println("result " + String(result));
			Serial.println("Subscribed with SID " + http.header("SID"));
			Serial.println(http.getString());
			http.end();
		});
	}
}

void loop() {
	server.handleClient();
}
