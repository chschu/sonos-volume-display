/*
 * Server.h
 *
 *  Created on: 30 Sep 2016
 *      Author: chschu
 */

#ifndef CONFIG_SERVER_H_
#define CONFIG_SERVER_H_

#include <ESP8266WebServer.h>
#include <IPAddress.h>
#include <cstdint>
#include <functional>

namespace Config {

class Persistent;

class Server {
public:
	typedef std::function<void()> Callback;

	Server(Persistent &config, IPAddress addr, uint16_t port = 80);
	Server(Persistent &config, uint16_t port = 80);

	void begin();
	void handleClient();
	void stop();

	// set the callback for cleanup before disconnecting from the current WiFi network
	void onBeforeNetworkChange(Callback callback);

	// set the callback for cleanup before disconnecting from the current WiFi network
	// the WiFi connection is probably not yet established when the callback is invoked
	void onAfterNetworkChange(Callback callback);

	// set the callback for cleanup before a configuration change
	void onBeforeConfigurationChange(Callback callback);

	// set the callback for initialization after a configuration change
	void onAfterConfigurationChange(Callback callback);

private:
	Persistent &_config;

	ESP8266WebServer _server;

	Callback _beforeNetworkChangeCallback;
	Callback _afterNetworkChangeCallback;

	Callback _beforeConfigurationChangeCallback;
	Callback _afterConfigurationChangeCallback;

	void _handleGetApiDiscoverNetworks();
	void _handleGetApiDiscoverRooms();
	void _handleGetApiNetwork();
	void _handlePostApiNetwork();
	void _handleGetApiConfig();
	void _handlePostApiConfig();
};

} /* namespace Config */

#endif /* CONFIG_SERVER_H_ */
