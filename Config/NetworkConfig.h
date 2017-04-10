#ifndef NETWORKCONFIG_H_
#define NETWORKCONFIG_H_

namespace Config {

class NetworkConfig {
public:
	typedef struct Data {
		char ssid[32];
		char passphrase[64];
	};

	NetworkConfig(Data &data);

	const char *ssid() const;
	bool setSsid(const char *ssid);

	const char *passphrase() const;
	bool setPassphrase(const char *passphrase);

	bool reset();

private:
	Data &_data;
};

} /* namespace Config */

#endif /* NETWORKCONFIG_H_ */
