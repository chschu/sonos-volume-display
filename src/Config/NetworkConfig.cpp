#include "NetworkConfig.h"

#include <Esp.h>
#include <WString.h>
#include <cstring>

namespace Config {

NetworkConfig::NetworkConfig(Data &data) : _data(data) {
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

const char *NetworkConfig::hostname() const {
    return _data.hostname;
}

bool NetworkConfig::setHostname(const char *hostname) {
    size_t len = strlen(hostname);
    if (len < 1 || len >= sizeof(_data.hostname)) {
        return false;
    }
    for (const char *p = hostname; *p; p++) {
        if (!isalnum(*p) && *p != '-') {
            return false;
        }
    }
    if (hostname[0] == '-' || hostname[len - 1] == '-') {
        return false;
    }
    strcpy(_data.hostname, hostname);
    return true;
}

bool NetworkConfig::reset() {
    String defaultHostname = String(F("svd-")) + String(ESP.getChipId(), 16);
    return setSsid("") && setPassphrase("") && setHostname(defaultHostname.c_str());
}

bool NetworkConfig::operator==(const NetworkConfig &other) const {
    return std::strncmp(_data.ssid, other._data.ssid, sizeof(_data.ssid)) == 0 &&
           std::strncmp(_data.passphrase, other._data.passphrase, sizeof(_data.passphrase)) == 0 &&
           std::strncmp(_data.hostname, other._data.hostname, sizeof(_data.hostname)) == 0;
}

bool NetworkConfig::operator!=(const NetworkConfig &other) const {
    return !(*this == other);
}

} /* namespace Config */
