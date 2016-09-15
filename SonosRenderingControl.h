/*
 * SonosRenderingControl.h
 *
 *  Created on: 12 Sep 2016
 *      Author: chschu
 */

#ifndef SONOSRENDERINGCONTROL_H_
#define SONOSRENDERINGCONTROL_H_

#include <IPAddress.h>
#include <cstdint>
#include <functional>

typedef std::function<void(uint16_t volume)> SonosGetVolumeCallback;

class SonosRenderingControl {
public:
	SonosRenderingControl(IPAddress deviceIP);
	~SonosRenderingControl();

	bool GetVolume(SonosGetVolumeCallback callback, uint32_t instanceID = 0, const char *channel = "Master");

private:
	IPAddress _deviceIP;
};

#endif /* SONOSRENDERINGCONTROL_H_ */
