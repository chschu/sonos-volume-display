#include "PersistentConfig.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <WString.h>
#include <cstddef>

namespace Config {

PersistentConfig::PersistentConfig(uint32_t magic) : _magic(magic) {
}

NetworkConfig PersistentConfig::network() {
    return NetworkConfig(_data.network);
}

SonosConfig PersistentConfig::sonos() {
    return SonosConfig(_data.sonos);
}

LedConfig PersistentConfig::led() {
    return LedConfig(_data.led);
}

static uint32_t crc32(const void *data, size_t length) {
    uint32_t crc = 0xffffffff;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(data);
    for (size_t i = length; i; i--, p++) {
        crc ^= *p;
        for (int8_t j = 7; j >= 0; j--) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return crc ^ 0xffffffff;
}

void PersistentConfig::load() {
    EEPROM.begin(sizeof(_data));
    EEPROM.get(0, _data);
    uint32_t checksum = crc32(&_data, offsetof(Data, checksum));
    if (_data.magic == _magic && _data.checksum == checksum) {
        Serial.println(F("magic number and checksum match, using configuration from EEPROM"));
    } else {
        Serial.println(F("magic number or checksum mismatch, initializing configuration in EEPROM"));
        Serial.printf("magic number expected %08X, got %08X\r\n", _magic, _data.magic);
        Serial.printf("checksum expected %08X, got %08X\r\n", checksum, _data.checksum);
        if (!reset()) {
            while (1) {
                Serial.println(F("initialization failed, going to sleep forever"));
                delay(10000);
            }
        }
        save();
    }
    EEPROM.end();
}

void PersistentConfig::save() {
    EEPROM.begin(sizeof(_data));
    _data.magic = _magic;
    _data.checksum = crc32(&_data, offsetof(Data, checksum));
    EEPROM.put(0, _data);
    EEPROM.end();
}

bool PersistentConfig::reset() {
    return network().reset() && sonos().reset() && led().reset();
}

} /* namespace Config */
