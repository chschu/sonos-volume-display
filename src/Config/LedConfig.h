#ifndef CONFIG_LEDCONFIG_H_
#define CONFIG_LEDCONFIG_H_

#include <cstdint>

#include "../Color/RGB.h"

namespace Config {

class LedConfig {
public:
	enum Transform {
		IDENTITY, // x -> x
		SQUARE, // x -> x^2
		SQUARE_ROOT, // x -> x^(1/2)
		INVERSE_SQUARE, // x -> 1-(1-x)^2
	};

	struct Data {
		uint8_t brightness;
		Transform transform;
	};

	LedConfig(Data &data);

	uint8_t brightness() const;
	bool setBrightness(uint8_t brightness);

	Transform transform() const;
	bool setTransform(Transform transform);

	bool reset();

private:
	Data &_data;
};

} /* namespace Config */

#endif /* CONFIG_LEDCONFIG_H_ */
