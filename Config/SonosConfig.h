#ifndef CONFIG_SONOSCONFIG_H_
#define CONFIG_SONOSCONFIG_H_

namespace Config {

class SonosConfig {
public:
	typedef struct Data {
		bool active;
		char roomUUID[32];
	};

	SonosConfig(Data &data);

	bool active() const;
	bool setActive(bool active);

	const char *roomUUID() const;
	bool setRoomUUID(const char *roomUUID);

	bool reset();

private:
	Data &_data;
};

} /* namespace Config */

#endif /* CONFIG_SONOSCONFIG_H_ */
