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
	typedef std::function<void()> BeforeNetworkChangeCallback;

	Server(Persistent *config, IPAddress addr, uint16_t port = 80);
	Server(Persistent *config, uint16_t port = 80);

	void begin();
	void handleClient();
	void stop();

	// set the callback for cleanup before reconnecting to a new WiFi network
	void onBeforeNetworkChange(BeforeNetworkChangeCallback callback);

private:
	Persistent *_config;

	ESP8266WebServer _server;
	BeforeNetworkChangeCallback _beforeNetworkChangeCallback;

	void _handleGetApiNetwork();
	void _handleGetApiNetworkCurrent();
	void _handlePostApiNetworkCurrent();
	void _handleGetApiRoom();
	void _handleGetApiRoomCurrent();
};

} /* namespace Config */

#endif /* CONFIG_SERVER_H_ */
