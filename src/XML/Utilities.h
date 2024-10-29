#ifndef XML_UTILITIES_H_
#define XML_UTILITIES_H_

#include <HardwareSerial.h>
#include <WString.h>
#include <functional>
#include <stddef.h>

namespace XML {

void replaceEntities(String &s);
bool extractEncodedTags(Stream &stream, const char *terminator, std::function<bool(String tag)> callback);
bool extractAttributeValue(String tag, String attributeName, String *attributeValue);

template <typename T> bool extractEncodedTags(Stream &stream, const char *terminator, std::function<bool(String tag, T userInfo)> callback, T userInfo) {
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

} // namespace XML

#endif /* XML_UTILITIES_H_ */
