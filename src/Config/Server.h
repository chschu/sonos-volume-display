#ifndef CONFIG_SERVER_H_
#define CONFIG_SERVER_H_

#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <IPAddress.h>
#include <WString.h>
#include <cstdint>
#include <functional>

#include "PersistentConfig.h"

namespace Config {

class PersistentConfig;

class Server {
  public:
    typedef std::function<void()> Callback;

    explicit Server(PersistentConfig &config, IPAddress addr, uint16_t port = 80);
    explicit Server(PersistentConfig &config, uint16_t port = 80);

    void begin();
    void handleClient();
    void stop();

    // set Network configuration change callbacks
    void onBeforeNetworkConfigChange(Callback callback);
    void onAfterNetworkConfigChange(Callback callback);

    // set Sonos Configuration change callbacks
    void onBeforeSonosConfigChange(Callback callback);
    void onAfterSonosConfigChange(Callback callback);

    // set LED Configuration change callbacks
    void onBeforeLedConfigChange(Callback callback);
    void onAfterLedConfigChange(Callback callback);

  private:
    PersistentConfig &_config;

    ESP8266WebServer _server;

    Callback _beforeNetworkConfigChangeCallback;
    Callback _afterNetworkConfigChangeCallback;

    Callback _beforeSonosConfigChangeCallback;
    Callback _afterSonosConfigChangeCallback;

    Callback _beforeLedConfigChangeCallback;
    Callback _afterLedConfigChangeCallback;

    void _handleGetApiInfo();

    void _handleGetApiDiscoverNetworks();
    void _handleGetApiDiscoverRooms();

    void _handleGetApiConfigNetwork();
    void _handlePostApiConfigNetwork();
    void _handleGetApiConfigSonos();
    void _handlePostApiConfigSonos();
    void _handleGetApiConfigLed();
    void _handlePostApiConfigLed();

    void _handlePostApiUpdate();
    void _handlePostApiUpdateUpload();

    void _sendResponseNetwork(int code);
    void _sendResponseSonos(int code);
    void _sendResponseLed(int code);

    void _sendResponseJson(int code, JsonVariantConst source);

    // extract the request argument, call a specialization of _convert(), and pass the result to the setter
    template <typename C, typename T> bool _handleArg(const String &name, C &config, bool (C::*setter)(T));

    // converter template, used by _handleArg()
    template <typename T> bool _convert(const String &input, T *output);
};

} /* namespace Config */

#endif /* CONFIG_SERVER_H_ */
