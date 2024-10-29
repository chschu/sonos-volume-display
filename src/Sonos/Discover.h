#ifndef SONOS_DISCOVER_H_
#define SONOS_DISCOVER_H_

class IPAddress;

namespace Sonos {

class Discover {
  public:
    // discover any Sonos device on the network via UPnP/SSDP
    // returns true if a device responds within the timeout, false otherwise
    // if addr is not NULL, the device's IP address is stored in *addr
    static bool any(IPAddress *addr, unsigned long timeoutMillis = 5000);
};

} // namespace Sonos

#endif /* SONOS_DISCOVER_H_ */
