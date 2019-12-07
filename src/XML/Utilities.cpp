#include <HardwareSerial.h>
#include <stddef.h>
#include <WString.h>
#include <functional>

#include "Utilities.h"

namespace XML {

void replaceEntities(String &s) {
	s.replace(F("&lt;"), F("<"));
	s.replace(F("&gt;"), F(">"));
	s.replace(F("&apos;"), F("'"));
	s.replace(F("&quot;"), F("\""));
	// TODO replace unicode entities with UTF-8 bytes
	s.replace(F("&amp;"), F("&"));
}

bool extractEncodedTags(Stream &stream, const char *terminator, std::function<bool(String tag)> callback) {
	return extractEncodedTags<int>(stream, terminator, [&callback](String tag, int) -> bool {
		return callback(tag);
	}, 0);
}

bool extractAttributeValue(String tag, String attributeName, String *attributeValue) {
	int attributeStart = tag.indexOf(' ' + attributeName + "=\"");
	if (attributeStart < 0) {
		Serial.println(F("Failed to find start of attribute"));
		return false;
	}
	int valueStart = attributeStart + attributeName.length() + 3;
	int valueEnd = tag.indexOf('"', valueStart);
	if (valueEnd < 0) {
		Serial.println(F("Failed to find end of attribute"));
		return false;
	}
	String value = tag.substring(valueStart, valueEnd);
	replaceEntities(value);
	*attributeValue = value;
	return true;
}

}
