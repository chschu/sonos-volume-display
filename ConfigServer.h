/*
 * ConfigServer.h
 *
 *  Created on: 30 Sep 2016
 *      Author: chschu
 */

#ifndef CONFIGSERVER_H_
#define CONFIGSERVER_H_

#include <ESP8266WebServer.h>
#include <IPAddress.h>
#include <cstdint>
#include <functional>

typedef std::function<void()> ConfigServerBeforeNetworkChangeCallback;

class ConfigServer {
public:
	ConfigServer(IPAddress addr, uint16_t port = 80);
	ConfigServer(uint16_t port = 80);
	~ConfigServer();

	void begin();
	void handleClient();
	void stop();

	// set the callback for cleanup before reconnecting to a new WiFi network
	void onBeforeNetworkChange(ConfigServerBeforeNetworkChangeCallback callback);

private:
	ESP8266WebServer _server;
	ConfigServerBeforeNetworkChangeCallback _beforeNetworkChangeCallback;

	void _handleGetApiNetwork();
	void _handleGetApiNetworkCurrent();
	void _handlePostApiNetworkCurrent();
	void _handleGetApiDiscover();
};

#endif /* CONFIGSERVER_H_ */
