#ifndef SONOS_RENDERINGCONTROL_H_
#define SONOS_RENDERINGCONTROL_H_

#include <IPAddress.h>
#include <cstdint>
#include <functional>

namespace Sonos {

class RenderingControl {
  public:
    typedef std::function<void(uint16_t volume)> GetVolumeCallback;

    RenderingControl(IPAddress deviceIP);

    bool GetVolume(GetVolumeCallback callback, uint32_t instanceID = 0, const char *channel = "Master");

  private:
    IPAddress _deviceIP;
};

} // namespace Sonos

#endif /* SONOS_RENDERINGCONTROL_H_ */
