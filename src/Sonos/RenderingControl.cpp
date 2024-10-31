#include "RenderingControl.h"

#include <ESP8266HTTPClient.h>
#include <HardwareSerial.h>
#include <WString.h>
#include <WiFiClient.h>
#include <cstring>
#include <pgmspace.h>
#include <stddef.h>
#include <stdlib.h>

namespace Sonos {

const char GET_VOLUME[] PROGMEM =
    "<?xml version=\"1.0\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
    "<s:Body>"
    "<u:GetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
    "<InstanceID>%u</InstanceID>"
    "<Channel>%s</Channel>"
    "</u:GetVolume>"
    "</s:Body>"
    "</s:Envelope>";

RenderingControl::RenderingControl(IPAddress deviceIP) : _deviceIP(deviceIP) {
}

bool RenderingControl::GetVolume(GetVolumeCallback callback, uint32_t instanceID, const char *channel) {
    size_t size = sizeof(GET_VOLUME) + (3 * sizeof(instanceID) - 2) + (strlen(channel) - 2);
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf_P(buf.get(), size, GET_VOLUME, instanceID, channel);

    WiFiClient wifiClient;
    HTTPClient client;
    client.begin(wifiClient, _deviceIP.toString(), 1400, F("/MediaRenderer/RenderingControl/Control"));
    client.addHeader(F("SOAPACTION"), F("urn:schemas-upnp-org:service:RenderingControl:1#GetVolume"));
    int status = client.POST(String(buf.get()));

    bool result = false;

    Serial.print(F("GetVolume returned HTTP status "));
    Serial.println(status);
    if (status == 200) {
        WiFiClient &stream = client.getStream();

        if (stream.find("<CurrentVolume>")) {
            // TODO read only until next '<' and check format
            long volume = stream.parseInt();
            // must fit into an uint16_t
            if (volume >= 0 && volume <= 65535) {
                callback(static_cast<uint16_t>(volume));
                result = true;
            }
        } else {
            Serial.println(F("GetVolume returned an unexpected response"));
        }
    }

    client.end();

    return result;
}

} // namespace Sonos
