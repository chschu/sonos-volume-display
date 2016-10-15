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
#include <WString.h>
#include <cstdint>

class ConfigServer {
public:
	ConfigServer(IPAddress addr, uint16_t port = 80);
	ConfigServer(uint16_t port = 80);
	~ConfigServer();

	void begin();
	void handleClient();
	void stop();

	bool needsReconnect();
	String reconnectSSID();
	String reconnectPassphrase();
	void reconnectDone();

private:
	ESP8266WebServer _server;

	void _handleGetApiNetwork();
	void _handleGetApiNetworkCurrent();
	void _handlePostApiNetworkCurrent();
	void _handleGetApiDiscover();

	String _reconnectSSID;
	String _reconnectPassphrase;
};

#endif /* CONFIGSERVER_H_ */
