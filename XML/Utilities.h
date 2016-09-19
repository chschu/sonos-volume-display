/*
 * Utilities.h
 *
 *  Created on: 19 Sep 2016
 *      Author: chschu
 */

#ifndef XML_UTILITIES_H_
#define XML_UTILITIES_H_

#include <HardwareSerial.h>
#include <stddef.h>
#include <WString.h>
#include <functional>

namespace XML {

void replaceEntities(String &s) {
	s.replace(F("&lt;"), F("<"));
	s.replace(F("&gt;"), F(">"));
	s.replace(F("&apos;"), F("'"));
	s.replace(F("&quot;"), F("\""));
	// TODO replace unicode entities with UTF-8 bytes
	s.replace(F("&amp;"), F("&"));
}

bool extractEncodedTags(Stream &stream, std::function<bool(String tag)> callback) {
	// skip everything up to (and including) the next &lt;
	while (stream.find("&lt;")) {
		String tag = "&lt;";

		// find the next &gt; that ends the encoded tag
		const char *endMarker = "&gt;";
		size_t index = 0;
		while (true) {
			char endMarkerCh = endMarker[index];
			if (!endMarkerCh) {
				// tag is complete, replace the XML entities
				replaceEntities(tag);
				if (!callback(tag)) {
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

}

#endif /* XML_UTILITIES_H_ */
