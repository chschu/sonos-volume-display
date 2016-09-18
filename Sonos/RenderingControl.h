/*
 * RenderingControl.h
 *
 *  Created on: 12 Sep 2016
 *      Author: chschu
 */

#ifndef SONOS_RENDERINGCONTROL_H_
#define SONOS_RENDERINGCONTROL_H_

#include <IPAddress.h>
#include <cstdint>
#include <functional>

namespace Sonos {

typedef std::function<void(uint16_t volume)> GetVolumeCallback;

class RenderingControl {
public:
	RenderingControl(IPAddress deviceIP);
	~RenderingControl();

	bool GetVolume(GetVolumeCallback callback, uint32_t instanceID = 0, const char *channel = "Master");

private:
	IPAddress _deviceIP;
};

}

#endif /* SONOS_RENDERINGCONTROL_H_ */
