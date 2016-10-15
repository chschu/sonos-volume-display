/*
 * Builder.h
 *
 *  Created on: 3 Oct 2016
 *      Author: chschu
 */

#ifndef JSON_BUILDER_H_
#define JSON_BUILDER_H_

#include <WString.h>

namespace JSON {

class Builder {
public:
	Builder();
	~Builder();

	// append a separator (',') if required, and start a JSON array ('[')
	void beginArray();
	// end a JSON array (']')
	void endArray();

	// append a separator (',') if required, and start a JSON object ('{')
	void beginObject();
	// end a JSON array ('}')
	void endObject();

	// append a separator (',') if required, and append a simple value
	void value(int x);
	void value(bool x);
	void value(const String &x);
	void value(const __FlashStringHelper *x);

	// append a separator (',') if required, and append a JSON attribute without value
	// this can be used to construct attributes with arbitrarily typed values
	void attribute(const String &name);

	// append a separator (',') if required, and write a JSON attribute with a simple value
	template<typename T>
	void attribute(const String &name, const T &x) {
		attribute(name);
		value(x);
	}

	// return a copy of the JSON string
	String toString();

private:
	String _json;

	void _separator();
	void _quoted(const String &x);
};

}

#endif /* JSON_BUILDER_H_ */
