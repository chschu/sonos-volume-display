#include "Builder.h"

#include <pgmspace.h>
#include <stddef.h>
#include <stdio.h>

namespace JSON {

void Builder::beginArray() {
	_separator();
	_json += '[';
}

void Builder::endArray() {
	_json += ']';
}

void Builder::beginObject() {
	_separator();
	_json += '{';
}

void Builder::endObject() {
	_json += '}';
}

void Builder::attribute(const String &name) {
	_separator();
	_quoted(name);
	_json += ':';
}

String Builder::toString() {
	return _json;
}

void Builder::value(int x) {
	_separator();
	_json += x;
}

void Builder::value(unsigned int x) {
	_separator();
	_json += x;
}

void Builder::value(bool x) {
	_separator();
	_json += x ? F("true") : F("false");
}

void Builder::value(const char *x) {
	_separator();
	_quoted(x);
}

void Builder::value(const String &x) {
	_separator();
	_quoted(x);
}

void Builder::value(const __FlashStringHelper *x) {
	_separator();
	_quoted(x);
}

void Builder::_separator() {
	unsigned length = _json.length();
	if (length > 0) {
		char lastChar = _json.charAt(length - 1);
		if (lastChar != '[' && lastChar != '{' && lastChar != ':') {
			_json += ',';
		}
	}
}

void Builder::_quoted(const String &x) {
	_json += '"';
	for (const char *p = x.c_str(); *p; p++) {
		switch (*p) {
		case '"':
		case '\\':
		case '/':
			_json += '\\';
			_json += *p;
			break;
		case '\b':
			_json += "\\b";
			break;
		case '\t':
			_json += "\\t";
			break;
		case '\n':
			_json += "\\n";
			break;
		case '\f':
			_json += "\\f";
			break;
		case '\r':
			_json += "\\r";
			break;
		default:
			if (*p < ' ') {
				char buf[7];
				snprintf_P(buf, sizeof(buf), PSTR("\\u%04X"), *p);
				_json += buf;
			} else {
				_json += *p;
			}
		}
	}
	_json += '"';
}

} /* namespace JSON */
