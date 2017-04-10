#ifndef CONFIG_SERVER_H_
#define CONFIG_SERVER_H_

#include <ESP8266WebServer.h>
#include <IPAddress.h>
#include <cstdint>
#include <functional>

namespace Config {

class PersistentConfig;

class Server {
public:
	typedef std::function<void()> Callback;

	Server(PersistentConfig &config, IPAddress addr, uint16_t port = 80);
	Server(PersistentConfig &config, uint16_t port = 80);

	void begin();
	void handleClient();
	void stop();

	// set the callback for cleanup before disconnecting from the current WiFi network
	void onBeforeNetworkChange(Callback callback);

	// set the callback for cleanup before disconnecting from the current WiFi network
	// the WiFi connection is probably not yet established when the callback is invoked
	void onAfterNetworkChange(Callback callback);

	// set Sonos Configuration change callbacks
	void onBeforeSonosConfigChange(Callback callback);
	void onAfterSonosConfigChange(Callback callback);

private:
	PersistentConfig &_config;

	ESP8266WebServer _server;

	Callback _beforeNetworkChangeCallback;
	Callback _afterNetworkChangeCallback;

	Callback _beforeSonosConfigChangeCallback;
	Callback _afterSonosConfigChangeCallback;

	void _handleGetApiDiscoverNetworks();
	void _handleGetApiDiscoverRooms();
	void _handleGetApiConfigNetwork();
	void _handlePostApiConfigNetwork();
	void _handleGetApiConfigSonos();
	void _handlePostApiConfigSonos();

	void _sendResponseSonos(int code);

	// extract the request argument, call a specialization of _convert(), and pass the result to the setter
	template<typename C, typename T>
	bool _handleArg(const String &name, C &config, bool (C::*setter)(T));

	// converter template, used by _handleArg()
	template<typename T>
	bool _convert(const String &input, T *output);
};

} /* namespace Config */

#endif /* CONFIG_SERVER_H_ */
