#include "NetworkConfig.h"

#include <cstring>

namespace Config {

NetworkConfig::NetworkConfig(Data &data) :
		_data(data) {
}

const char *NetworkConfig::ssid() const {
	return _data.ssid;
}

bool NetworkConfig::setSsid(const char *ssid) {
	if (strlen(ssid) >= sizeof(_data.ssid)) {
		return false;
	}
	strcpy(_data.ssid, ssid);
	return true;
}

const char *NetworkConfig::passphrase() const {
	return _data.passphrase;
}

bool NetworkConfig::setPassphrase(const char *passphrase) {
	if (strlen(passphrase) >= sizeof(_data.passphrase)) {
		return false;
	}
	for (const char *p = passphrase; *p; p++) {
		if (*p < 32 || *p > 126) {
			return false;
		}
	}
	strcpy(_data.passphrase, passphrase);
	return true;
}

bool NetworkConfig::reset() {
	return setSsid("") && setPassphrase("");
}

} /* namespace Config */
