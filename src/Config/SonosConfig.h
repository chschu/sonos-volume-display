#ifndef CONFIG_SONOSCONFIG_H_
#define CONFIG_SONOSCONFIG_H_

namespace Config {

class SonosConfig {
public:
	typedef struct Data {
		bool active;
		char roomUuid[32];
	};

	SonosConfig(Data &data);

	bool active() const;
	bool setActive(bool active);

	const char *roomUuid() const;
	bool setRoomUuid(const char *roomUuid);

	bool reset();

private:
	Data &_data;
};

} /* namespace Config */

#endif /* CONFIG_SONOSCONFIG_H_ */
