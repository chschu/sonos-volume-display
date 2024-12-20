#ifndef CONFIG_PERSISTENTCONFIG_H_
#define CONFIG_PERSISTENTCONFIG_H_

#include <cstdint>

#include "LedConfig.h"
#include "NetworkConfig.h"
#include "SonosConfig.h"

namespace Config {

class PersistentConfig {
  public:
    struct Data {
        uint32_t magic;
        NetworkConfig::Data network;
        SonosConfig::Data sonos;
        LedConfig::Data led;
        uint32_t checksum;
    };

    explicit PersistentConfig(uint32_t magic = 0x51DEB00B);

    // "views" with validating setters, working on the actual configuration data
    NetworkConfig network();
    SonosConfig sonos();
    LedConfig led();

    // (re-)load configuration from EEPROM
    // if magic number or checksum don't match, initialize defaults and store in EEPROM
    void load();

    // store configuration in EEPROM
    void save();

    // reset _data to defaults
    // doesn't update EEPROM
    bool reset();

  private:
    uint32_t _magic;
    Data _data;
};

} /* namespace Config */

#endif /* CONFIG_PERSISTENTCONFIG_H_ */
