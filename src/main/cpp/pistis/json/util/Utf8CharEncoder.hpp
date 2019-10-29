#ifndef __PISTIS__JSON__UTIL__UTF8CHARENCODER_HPP__
#define __PISTIS__JSON__UTIL__UTF8CHARENCODER_HPP__

#include <pistis/exceptions/IllegalValueError.hpp>
#include <iomanip>
#include <sstream>

namespace pistis {
  namespace json {
    namespace util {

      class Utf8CharEncoder {
      public:

	template <typename Buffer>
	void encodeChar(Buffer& buffer, uint32_t c) const {
	  if (c < 0x80) {
	    buffer.write((char)c);
	  } else if (c < 0x800) {
	    buffer.write((char)(0xC0 | (c >> 6)));
	    buffer.write((char)(0x80 | (c & 0x3F)));
	  } else if (c < 0x10000) {
	    buffer.write((char)(0xE0 | (c >> 12)));
	    buffer.write((char)(0x80 | ((c >> 6) & 0x3F)));
	    buffer.write((char)(0x80 | (c & 0x3F)));
	  } else if (c < 0x200000) {
	    buffer.write((char)(0xF0 | (c >> 18)));
	    buffer.write((char)(0x80 | ((c >> 12) & 0x3F)));
	    buffer.write((char)(0x80 | ((c >> 6) & 0x3F)));
	    buffer.write((char)(0x80 | (c & 0x3F)));
	  } else {
	    std::ostringstream msg;
	    msg << "Cannot encode character 0x" << std::width(4)
		<< std::fill('0') << std::hex << c
		<< ", which has more than 21 bits";
	    throw pistis::exceptions::IllegalValueError(msg.str(),
							PISTIS_EX_HERE);
	  }
	}
	
      };
      
    }
  }
}
#endif

