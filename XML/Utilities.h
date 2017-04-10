#ifndef XML_UTILITIES_H_
#define XML_UTILITIES_H_

#include <HardwareSerial.h>
#include <stddef.h>
#include <WString.h>
#include <functional>

namespace XML {

static void replaceEntities(String &s) {
	s.replace(F("&lt;"), F("<"));
	s.replace(F("&gt;"), F(">"));
	s.replace(F("&apos;"), F("'"));
	s.replace(F("&quot;"), F("\""));
	// TODO replace unicode entities with UTF-8 bytes
	s.replace(F("&amp;"), F("&"));
}

template<typename T>
static bool extractEncodedTags(Stream &stream, const char *terminator,
		std::function<bool(String tag, T userInfo)> callback, T userInfo) {
	// skip everything up to (and including) the next &lt;
	while (stream.findUntil("&lt;", terminator)) {
		String tag = "&lt;";

		// find the next &gt; that ends the encoded tag
		const char *endMarker = "&gt;";
		size_t index = 0;
		while (true) {
			char endMarkerCh = endMarker[index];
			if (!endMarkerCh) {
				// tag is complete, replace the XML entities
				replaceEntities(tag);
				if (!callback(tag, userInfo)) {
					Serial.println(F("Callback returned false"));
					return false;
				}
				break;
			}

			// use stream.readBytes() to do a timed read
			char ch;
			int cnt = stream.readBytes(&ch, 1);
			if (!cnt) {
				Serial.println(F("Stream ended unexpectedly"));
				return false;
			}

			// continuously match endMarker
			if (ch == endMarkerCh) {
				index++;
			} else {
				// reset after failed match
				index = 0;
			}

			// append read character
			tag += ch;
		}
	}

	return true;
}

static bool extractEncodedTags(Stream &stream, const char *terminator, std::function<bool(String tag)> callback) {
	return extractEncodedTags<int>(stream, terminator, [&callback](String tag, int) -> bool {
		return callback(tag);
	}, 0);
}

static bool extractAttributeValue(String tag, String attributeName, String *attributeValue) {
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

#endif /* XML_UTILITIES_H_ */
