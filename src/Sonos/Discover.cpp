#include "../UPnP/Discover.h"
#include "Discover.h"

#include <IPAddress.h>
#include <Stream.h>

namespace Sonos {

bool Discover::any(IPAddress *deviceIP, unsigned long timeoutMillis) {
    bool deviceFound = false;
    return UPnP::Discover::all(
               [&deviceFound, deviceIP](IPAddress remoteIP, Stream &stream) -> bool {
                   if (!stream.find("Sonos", 5)) {
                       // this is not a Sonos response; wait for the next one
                       return true;
                   }
                   if (deviceIP) {
                       *deviceIP = remoteIP;
                   }
                   deviceFound = true;
                   // device found, stop discovery
                   return false;
               },
               "urn:schemas-upnp-org:device:ZonePlayer:1", 1) &&
           deviceFound;
}

} // namespace Sonos
