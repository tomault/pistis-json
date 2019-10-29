#ifndef __PISTIS__JSON__UTIL__JSONSTRINGDECODER_HPP__
#define __PISTIS__JSON__UTIL__JSONSTRINGDECODER_HPP__

#include <pistis/json/memory/StringBuffer.hpp>

namespace pistis {
  namespace json {
    namespace util {

      template <typename CharEncoder, typename Allocator>
      class JsonStringDecoder : CharEncoder {
      public:
	typedef Allocator AllocatorType;
	typedef memory::StringBuffer<Allocator> StringBufferType;
	
      public:
	JsonStringDecoder(memory::StringBuffer<Allocator>* stringBuffer):
	    stringBuffer_(stringBuffer) {
	}

	void setBuffer(memory::StringBuffer<Allocator>* stringBuffer) {
	  stringBuffer_ = stringBuffer;
	}

	
	JsonString operator()(const JsonString& input) {
	  const char* p = input.begin();
	  const char* last = p;
	  uint32_t c;

	  stringBuffer_->clear();
	  while (p != input.end()) {
	    if ((*p == '\\') && ((p + 1) < input.end())) {
	      stringBuffer_->write(last, p);
	      ++p;
	      if (*p == 'n') {
		stringBuffer_->write('\n');
	      } else if (*p == 't') {
		stringBuffer_->write('\t');
	      } else if (*p == 'r') {
		stringBuffer_->write('\r');
	      } else if (*p == 'u') {
		std::tie(p, c) = decodeHexChar_(p, input.end());
		this->encode(stringBuffer_, c);
	      } else if (*p == 'b') {
		stringBuffer_->write('\b');
	      } else {
		stringBuffer_->write(*p);
	      }
	      last = p + 1;
	    }
	    ++p;
	  }

	  if (last == input.begin()) {
	    return input;
	  } else {
	    stringBuffer_->write(last, input.end());
	    return JsonString(stringBuffer_->begin(), stringBuffer_->end());
	  }
	}
	
      private:
	memory::StringBuffer<Allocator> stringBuffer_;

	std::tuple<const char*, uint32_t> decodeHexChar_(const char* p,
							 const char* end) {
	  const char* const last = std::min(p + 4, end);
	  uint32_t c = 0;

	  while (p < last) {
	    if ((*p >= '0') && (*p <= '9')) {
	      c = (c << 4) | (*p - '0');
	    } else if ((*p >= 'A') && (*p <= 'F')) {
	      c = (c << 4) | (*p - 'A' + 10);
	    } else if ((*p >= 'a') && (*p <= 'f')) {
	      c = (c << 4) | (*p - 'a' + 10);
	    } else {
	      break;
	    }
	  }
	  
	  return std::make_tuple(p, c);
	}
      };
      
    }
  }
}
#endif
