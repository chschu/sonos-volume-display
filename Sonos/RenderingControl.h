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
	~RenderingControl();

	bool GetVolume(GetVolumeCallback callback, uint32_t instanceID = 0, const char *channel = "Master");

private:
	IPAddress _deviceIP;
};

}

#endif /* SONOS_RENDERINGCONTROL_H_ */
