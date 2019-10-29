#ifndef __PISTIS__JSON__STREAMING__FLEXIBLESTREAMREADER_HPP__
#define __PISTIS__JSON__STREAMING__FLEXIBLESTREAMREADER_HPP__

#include <pistis/exceptions/IllegalStateError.hpp>
#include <pistis/json/exceptions/JsonStringNotTerminated.hpp>
#include <pistis/json/exceptions/InvalidJsonNumberError.hpp>
#include <pistis/json/streaming/JsonEventOrigin.hpp>
#include <pistis/json/streaming/JsonEventType.hpp>
#include <pistis/json/streaming/JsonLookAhead.hpp>
#include <cctype>
#include <memory>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pistis {
  namespace json {
    namespace streaming {

      template <typename Stream, typename CharEncoder, typename Allocator>
      class FlexibleStreamReader : Allocator, CharEncoder {
      public:
	FlexibleStreamReader(Stream&& stream, size_t chunkSize,
			     const CharEncoder charEncoder = CharEncoder(),
			     const Allocator& allocator = Allocator()):
	    Allocator(allocator), CharEncoder(charEncoder),
	    buffer_(this->allocate(chunkSize_)),
	    stream_(std::move(stream)), stringBuffer_(), chunkSize_(chunkSize),
	    bufferExtensionLimit_(chunkSize_ - (chunkSize_ >> 8)),
	    bufferEos_(buffer_.get() + chunkSize_), bufferEnd_(buffer_.get()),
	    current_(buffer_.get()), bufferOffset_(0), lineStartOffset_(0),
	    lineNumber_(1), saved_(), numberParseState_(0),
	    numberEventType_(JsonEventType::INT_VALUE), lastBuffer_(nullptr) {
	}
	FlexibleStreamReader(const FlexibleStreamReader&) = delete;
	FlexibleStreamReader(FlexibleStreamReader&&) = default;

	JsonEventOrigin position() const {
	  return JsonEventOrigin(lineNumber_, (current_ - lineStart_ + 1),
				 bufferOffset_ + (current_ - buffer_.get()));
	}
	
	JsonLookAhead lookAhead() {
	  while (true) {
	    if (current_ == bufferEnd_) {
	      switch(fillBuffer_()) {
	        case FillResult::AGAIN: return JsonLookAhead::AGAIN;
	        case FillResult::END_OF_STREAM:
		  return JsonLookAhead::END_OF_STREAM;
	      }
	    }

	    const char c = *current_;
	    if (c == '\n') {
	      lineStartOffset_ = bufferOffset_ + (current_ - buffer_.get());
	      ++lineNumber_;
	    } else if ((c != ' ') && (c != '\t') && (c != '\n') &&
		       (c != '\r')) {
	      return JsonLookAhead(c);
	    }
	    ++current_;
	  }
	}
	
	void advance() { ++current_; }
	
	bool nextString(JsonString& text, JsonEventOrigin& origin,
			bool resumed = false) {
	  if ((current_ == bufferEnd_) || (*current_ != '"')) {
	    throw IncorrectAlignment("Must be aligned to a double-quote",
				     position(), PISTIS_EX_HERE);
	  }

	  SavedState startingState(*this);
	  
	  origin = position();

	  if (resumed) {
	    current_ = saved_.restore(*this);
	  } else {
	    stringBuffer_.clear();
	    lastBuffer_ = nullptr;
	    ++current_;
	  }

	  while (true) {
	    if (current_ == bufferEnd_) {
	      switch(fillBuffer_(startingState)) {
	        case FillResult::AGAIN:
		  saved_.save(*this);
		  startingState.restore(*this);
		  return false;

	        case FillResult::END_OF_STREAM:
		  throw exceptions::JsonStringNotTerminated(position(),
							    PISTIS_EX_HERE);
	      }
	    }

	    if (*current_ == '"') {
	      if (lastBuffer_) {
		text = JsonString(stringBuffer_.begin(), stringBuffer_.end());
	      } else {
		text = JsonString(startingState.current() + 1, current_);
	      }
	      ++current_; // Consume the '"'
	      return true;
	    } else if (*current_ == '\n') {
	      lineStartOffset_ = bufferOffset_ + (current_ - buffer_.get() + 1);
	      ++lineNumber_;
	    } else if (*currnet_ == '\\') {
	      if (!lastBuffer_) {
		lastBuffer_ = initialState.current() + 1;
	      }
	      if (current_ > lastBuffer_) {
		stringBuffer_.write(lastBuffer_, current_);
	      }
	      if (decodeEscapeSequence_(initialState) == DecodeResult::AGAIN) {
		lastBuffer_ = current_;
		saved_.save(*this);
		initialState.restore(*this);
		return false;
	      } else {
		lastBuffer_ = current_;
		--current_; // Will be undone by the ++current_ below
	      }
	    }
	    ++current_;
	  }
	}
	
	bool nextNumber(JsonEventType& eventType, JsonString& payload,
			JsonEventOrigin& origin, bool restarted = false) {
	  if ((current_ == bufferEnd_) ||
	      !(std::isdigit(*current_) || (*current_ == '-'))) {
	    throw IncorrectAlignment("Must be aligned to a number",
				     position(), PISTIS_EX_HERE);
	  }

	  SavedState initialState(*this);	  

	  
	  origin = position();
	  if (restarted) {
	    saved_.restore(*this);
	  } else {
	    ++current_;
	    numberParseState_ = 1;
	    numberEventType_ = JsonEventType::INT_VALUE;
	  }

	  while (numberParseState_) {
	    switch(numberParseState_) {
	      case 1: // Parsing initial digits
		switch(scanDigits_(numberStart, initialState)) {
		  case DigitScanResult::NONE:
		  case DigitScanResult::END_OF_STREAM:
		    throw IncorrectAlignment("Must be aligned to a number",
					     position(), PISTIS_EX_HERE);
		  case DigitScanResult::AGAIN:
		    saved_.save(*this);
		    initialState.restore(*this);
		    return false;
		}
		numberParseState_ = 2;
		// Fall through

	      case 2: // Checking for '.', 'e' or 'E'
		if ((current_ == bufferEnd_) &&
		    (fillBuffer_(initialState) ==
		        FillResult::AGAIN)) {
		  saved_.save(*this);
		  initialState.restore(*this);
		  return false;
		}
		if (current_ == bufferEnd_) {
		  numberParseState_ = 0; // Nothing more to read
		  break;
		} else if ((*current_ == 'e') || (*current == 'E')) {
		  numberEventType_ = JsonEventType::FLOAT_VALUE;
		  numberParseState_ = 5;
		  break;
		} else if (*current_ != '.') {
		  numberParseState_ = 0;  // Hit end of number
		  break;
		} else {
		  numberEventType_ = JsonEventType::FLOAT_VALUE;
		  numberParseState_ = 3;
		  ++current_;
		}
		// Fall through to state 3

	      case 3: // Reading digits after '.'
		switch(scanDigits_(numberStart, initialState)) {
		  case DigitScanResult::NONE:
		  case DigitScanResult::END_OF_STREAM:
		    throw exceptions:: InvalidJsonNumberError(origin,
							      PISTIS_EX_HERE);

		  case DigitScanResult::AGAIN:
		    saved_.save(*this);
		    initialState.restore(*this);
		    return false;
		}
		numberParseState_ = 4;
		// Fall through to state 4

	      case 4: // Checking for 'E' or 'e' after digits following '.'
		if ((current_ == bufferEnd_) &&
		    fillBuffer_(initialState) ==
		       FillResult::AGAIN) {
		  saved_.save(*this);
		  initialState.restore(*this);
		  return false;
		}
		if ((current_ == bufferEnd_) ||
		    ((*current_ != 'e') && (*current != 'E'))) {
		  numberParseState_ = 0;
		  break;
		} else {
		  numberParseState_ = 5;
		  ++current_;
		}

	      case 5: // Checking for '+' or '-' after 'E' or 'e'
		if ((current_ == bufferEnd_) &&
		    fillBuffer_(initialState) ==
		       FillResult::AGAIN) {
		  saved_.save(*this);
		  initialState.restore(*this);
		  return false;
		}
		if (current_ == bufferEnd_) {
		  throw exceptions::InvalidJsonNumberError(origin,
							   PISTIS_EX_HERE);
		} else {
		  if ((*current_ == '+') || (*current_ == '-')) {
		    ++current_;
		  }
		  numberParseState_ = 6;
		}
		// Fall through to state 6

	      case 6:  // Reading digits after 'E', 'e', '+' or '-'
		switch(scanDigits_(numberStart, initialState)) {
		  case DigitScanResult::NONE:
		  case DigitScanResult::END_OF_STREAM:
		    throw exceptions::InvalidJsonNumberError(origin,
							     PISTIS_EX_HERE);

		  case DigitScanResult::AGAIN:
		    saved_.save(*this);
		    initialState.restore(*this);
		    return false;
		}
		numberParseState_ = 0;
		break;

	      default:
		throw pistis::exception::IllegalStateError(
		    "Entered invalid state", PISTIS_EX_HERE
		);
	    }
	  }

	  eventType = numberEventType_;
	  payload = JsonString(initialState.current(), current_);
	  return true;
	}
	
	bool recognizeWord(const char* word, size_t length,
			   JsonEventOrigin& origin, bool restart) {
	  if ((current_ == bufferEnd_) || (*current != word[0])) {
	    throw IncorrectAlignment("Not aligned to word", location(),
				     PISTIS_EX_HERE);
	  }

	  SavedState initialState(*this);
	  origin = position();
	  
	  if (restart) {
	    saved_.restore(*this);
	  }
	  while ((bufferEnd_ - current_) < length) {
	    switch(fillBuffer_(initialState)) {
	      case FillResult::AGAIN:
		saved_.save(*this);
		initialState.restore(*this);
		return false;

	      case FillResult::END_OF_STREAM:
		throw IncorrectAlignment("Not aligned to word", origin,
					 PISTIS_EX_HERE);
	    }
	  }

	  while ((bufferEnd_ - current_) < (length + 1)) {
	    switch(fillBuffer_(initialState)) {
	      case FillResult::AGAIN:
		saved_.save(*this);
		initialState.restore(*this);
		return false;

	      case FillResult::END_OF_STREAM:
		if (::memcmp(word, current_, length)) {
		  throw IncorrectAlignment("Not aligned to word", origin,
					   PISTIS_EX_HERE);
		}
		return true;
	    }
	  }

	  if (::memcmp(word, current_, length) || std::isalnum(word[length])) {
	    throw IncorrectAlignment("Not aligned at word", origin,
				     PISTIS_EX_HERE);
	  }
	  current_ += length;
	  return true;
	}
	
	FlexibleStreamReader& operator=(const FlexibleStreamReader&) = delete;
	FlexibleStreamReader& operator=(FlexibleStreamReader&&) = default;

      private:
	enum class FillResult {
	  FILLED,        ///< Data was read into the buffer
	  AGAIN,         ///< No data available, but more available later
	  END_OF_STREAM  ///< No data available, end of stream reached
	};

	enum class DigitScanResult {
	  NONE,         ///< Text at current_ is not a digit
	  READ,         ///< Read one or more digits
	  AGAIN,        ///< No data available, but more available later
	  END_OF_STREAM ///< Reached the end of the stream
	};

	enum class DecodeResult {
	  AGAIN,    ///< No data available, but more available later
	  DECODED   ///< Decoded escape sequence successfully
	};

	class SavedState {
	public:
	  SavedState():
	      current_(nullptr), lineStart_(nullptr), lineNumber_(0) {
	  }
	  SavedState(const FlexibleStreamReader& reader):
	      current_(reader.current_), lineStart_(reader.lineStart_)
	      lineNumber_(reader.lineNumber_) {
	  }
	  SavedState(const SavedState&) = default;

	  void save(const FlexibleStreamReader& reader) {
	    current_ = reader.current_;
	    lineStart_ = reader.lineStart_;
	    lineNumber_ = reader.lineNumber_;
	  }

	  void restore(FlexibleStreamReader& reader) {
	    reader.current_ = current_;
	    reader.lineStart_ = lineStart_;
	    reader.lineNumber_ = lineNumber_;
	  }

	  const char* current() const { return current_; }
	  void setCurrent(const char* p) { current_ = p; }

	  SavedState& operator=(const SavedState& other) = default;
	  
	private:
	  const char* current_;
	  uint64_t lineStartOffset_;
	  uint64_t lineNumber_;
	};

	
      private:
	std::unique_ptr<char[]> buffer_;
	Stream stream_;
	memory::StringBuffer stringBuffer_;
	size_t chunkSize_;
	size_t bufferExtensionLimit_;
	const char* bufferEos_;
	const char* bufferEnd_;
	const char* current_;
	uint64_t bufferOffset_;
	uint64_t lineStartOffset_;
	uint64_t lineNumber_;
	SavedState saved_;
	uint32_t numberParseState_;
	const char* lastBuffer_;

	FillResult fillBuffer_() {
	  ::assert(current_ == bufferEnd_);

	  bufferOffset_ += (bufferEnd_ - buffer_.get());
	  bufferEnd_ = buffer_.get();
	  current_ = bufferEnd_;

	  ssize_t n = buffer_.read(buffer_.get(), bufferEos_ - bufferEnd_);
	  if (n < 0) {
	    return FillResult::AGAIN;
	  } else if (!n) {
	    return FillResult::END_OF_STREAM;
	  } else {
	    bufferEnd_ += n;
	    return FillResult::FILLED;
	  }
	}
	
	FillResult fillBuffer_(SavedState& initialState) {
	  ::assert(current_ == bufferEnd_);
	  const char* const preserve = initialState.current();
	  const size_t numToKeep = bufferEnd_ - preserve;
	  const size_t numToRemove = preserve - buffer_.get();
	  size_t numToRead = bufferEos_ - numToKeep;
	  
	  if (numToRead >= bufferExtensionLimit_) {
	    // Move stuff down and fill remaining space in the buffer
	    ::memcpy(buffer_.get(), preserve, numToKeep);
	    bufferEnd_ -= numToRemove;
	    current_ = bufferEnd_;
	    bufferOffset_ += numToRemove;
	    initialState.setCurrent(buffer_.get());
	  } else {
	    // Move stuff down and extend the buffer enough so we can fit
	    // chunkSize_ more bytes in it
	    const size_t newBufferSize = chunkSize_ - numToRead_;
	    std::unique_ptr<char[]> newBuffer(this->allocate(newBufferSize));

	    ::memcpy(newBuffer_.get(), preserve, numToKeep);
	    bufferEos_ = newBuffer_.get() + newBufferSize;
	    bufferEnd_ = newBuffer_.get() + numToKeep;
	    current_ = bufferEnd_;
	    bufferOffset_ += numToRemove;
	    initialState.setCurrent(newBuffer_.get());
	    buffer_ = std::move(newBuffer);
	    numToRead = chunkSize_;
	  }

	  ssize_t n = stream_.read(bufferEnd_, numToRead);
	  if (n < 0) {
	    return FillResult::AGAIN;
	  } else if (!n) {
	    return FillResult::END_OF_STREAM;
	  } else {
	    bufferEnd_ += n;
	    return FillResult::FILLED;
	  }
	}

	DigitScanResult scanDigits_(SavedState& initialState) {
	  const char* start = current_;
	  while (true) {
	    if (current_ == bufferEnd_) {
	      uint64_t oldOffset = bufferOffset_;
	      switch (fillBuffer_(initialState)) {
	        case FillResult::AGAIN: return DigitScanResult::AGAIN;
	        case FillResult::END_OF_STREAM:
		  return (current_ > start) ? DigitScanResult::READ
		                            : DigitScanResult::END_OF_STREAM;
	      }
	      start -= bufferOffset_ - oldOffset;
	    }

	    if (std::isdigit(*current_)) {
	      ++current_;
	    } else if (current_ > start) {
	      return DigitScanResult::READ;
	    } else {
	      return DigitScanResult::NONE;
	    }
	  }
	}

	DecodeResult decodeEscapeSequence_(SavedState& initialState) {
	  ++current_; // Consume '\'
	  if (current_ == bufferEnd_) {
	    switch(fillBuffer_(initialState)) {
	      case FillResult::AGAIN:
		--current_; // Put back the '\'
		return DecodeResult::AGAIN;
		
	      case FillResult::END_OF_STREAM:
		throw InvalidJsonStringError("Invalid escape sequence",
					     position(), PISTIS_EX_HERE);
	    }
	  }
	  switch(*current_) {
	    case '"':
	    case '\\':
	    case '/':
	      stringBuffer_.write(*current_);
	      break;

	    case 'b':
	      stringBuffer_.write('\b');
	      break;

	    case 'f':
	      stringBuffer_.write('\f');
	      break;

	    case 'n':
	      stringBuffer_.write('\n');
	      break;

	    case 'r':
	      stringBuffer_.write('\r');
	      break;

	    case 't':
	      stringBuffer_.write('\t');
	      break;

	    case 'u':
	      return decodeHexSequence_(initialState);

	    default:
	      throw InvalidJsonStringError("Invalid escape sequence",
					   position(), PISTIS_EX_HERE);
		  
	  }
	  return DecodeResult::DECODED;
	}

	DecodeResult decodeHexSequence_(SavedState& initialState) {
	  ++current_; // Consume the 'u'
	  while ((bufferEnd_ - current_) < 4) {
	    switch (fillBuffer_(initialState)) {
	      case FillResult::AGAIN:
		current_ -= 2; // Back up to '\' and restart later
		return DecodeResult::AGAIN;

	      case FillResult::END_OF_STREAM:
		// Illegal escape sequence
		throw InvalidJsonStringError("Invalid escape sequence \"\\u\"",
					     position(), PISTIS_EX_HERE);
	    }
	  }
	  const uint32_t c = (((((decodeHexChar_(current_[0]) << 8) ||
				 decodeHexChar_(current_[1])) << 8) ||
			       decodeHexChar_(current_[2])) << 8) ||
	                     decodeHexChar_(current_[3]);
	  if (c > 0x001FFFFF) {
	    throw InvalidJsonStringError("Not a legal unicode character",
					 position(), PISTIS_EX_HERE);
	  }
	  this->encodeChar(stringBuffer_, c);
	  current_ += 4;
	  return DecodeResult::DECODED;
	}

	static bool isEscapedQuote_(const char* start, const char* buffer) {
	  if ((start >= buffer) && (*start == '\\')) {
	    const char* p = start - 1;
	    while ((p >= buffer) && (*p == '\\')) {
	      --p;
	    }
	    return (start - p) & 0x1;
	  }
	  return false;
	}
      };
      
    }
  }
}

#endif
