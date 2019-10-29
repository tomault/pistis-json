/** @file FlexibleEventStream.hpp
 *
 *  A stream of JSON events, with payload types constructed by a factory.
 */
#ifndef __PISTIS__JSON__STREAMING__FLEXIBLEEVENTSTREAM_HPP__
#define __PISTIS__JSON__STREAMING__FLEXIBLEEVENTSTREAM_HPP__

#include <pistis/exceptions/IllegalStateError.hpp>
#include <pistis/json/InvalidJsonStringError.hpp>
#include <pistis/json/streaming/FlexibleStreamReader.hpp>
#include <pistis/json/streaming/JsonEventType.hpp>
#include <pistis/json/streaming/JsonEventOrigin.hpp>
#include <pistis/json/streaming/JsonLookAhead.hpp>
#include <pistis/json/streaming/Utf8CharEncoder.hpp>
#include <memory>
#include <type_traits>
#include <vector>

namespace pistis {
  namespace json {
    namespace streaming {

      template <typename Stream, typename PayloadFactory,
		typename CharEncoder = util::Utf8CharEncoder,
		typename Allocator = std::allocator<char> >
      class FlexibleEventStream {
      public:
	typedef typename std::remove_cv<
	            typename std::remove_reference<
		        decltype(((*PayloadFactory*)0)->
				     intValue((*(JsonString*)0)))
	            >::type
	        >::type
	        IntPayloadType;
	typedef typename std::remove_cv<
	            typename std::remove_reference<
		        decltype(((*PayloadFactory*)0)->
				     floatValue((*(JsonString*)0)))
	            >::type
	        >::type
	        FloatPayloadType;
	typedef typename std::remove_cv<
	            typename std::remove_reference<
		        decltype(((*PayloadFactory*)0)->
				     stringValue((*(JsonString*)0)))
	            >::type
	        >::type
	        StringPayloadType;
	
      private:
	class State {
	public:
	  State(): f_(nullptr), event_(JsonEventType::AGAIN) { }
	  State(State (FlexibleEventStream::* f)(), JsonEventType event)
	      : f_(f), event_(event) {
	  }

	  State next(FlexibleEventStream* s) const { return (s->*f_)(); }
	  JsonEventType event() const { return event_; }
	  operator bool() const { return (bool)next_; }
	    
	private:
	  State (FlexibleEventStream::* f_)();
	  JsonEventType event_;
	};

	typedef FlexibleStreamReader<Stream, Allocator>::IncorrectAlignment
	        IncorrectAlignmentError;
	
      public:
	FlexibleEventStream(const std::string& streamName,
			    Stream&& stream,
			    const PayloadFactory& payloadFactory,
			    size_t bufferSize)
	    : streamName_(streamName), reader_(std::move(stream), bufferSize),
	      payloadFactory_(std::move(payloadFactory)),
	      payload_(), origin_(0, 0, 0),
	      current_(&FlexibleEventStream::parseInitialValue_,
		       JsonEventType::AGAIN),
	      stateStack_() {
	}
	
	FlexibleEventStream(const FlexibleEventStream&) = delete;
	FlexibleEventStream(FlexibleEventStream&&) = default;

	JsonEventType next() {
	  if (!current_) {
	    return JsonEventType::END;
	  } else {
	    current_ = current_.next();
	    return current_.event();
	  }
	}

	const JsonEventOrigin& origin() { return origin_; }
	const JsonString& payloadText() const { return payload_; }
	auto intPayload() const { return payloadFactory_.intValue(payload_); }
	auto floatPayload() const {
	  return payloadFactory_.floatValue(payload_);
	}
	auto stringPayload() const {
	  return payloadFactory_.stringValue(payload_);
	}

	template <typename ArrayBuilderFactory, typename ObjectBuilderFactory>
	auto readObject(const ArrayBuilderFactory& createArrayBuilder,
			const ObjectBuilderFactory& createObjectBuilder) {
	  // Assume positioned at the start of an object
	  auto builder = createObjectBuilder(origin());
	  JsonEventType evt;

	  while ((evt = next()) != JsonEventType::END_OBJECT) {
	    switch(evt) {
	      case JsonEventType::AGAIN:
		break;

	      case JsonEventType::FIELD_NAME:
		readObjectValue_(builder, stringPayload(),
				 createArrayBuilder, createObjectBuilder);
		break;

	      default:
		// Should not happen
	    }
	  }
	  return builder.done();
	}

	template <typename ArrayBuilderFactory, typename ObjectBuilderFactory>
	auto readArray(const ArrayBuilderFactory& createArrayBuilder,
		       const ObjectBuilderFactory& createObjectBuilder) {
	  auto builder = createArrayBuilder(origin());
	  JsonEventType evt;

	  while ((evt == next()) != JsonEventType::END_ARRAY) {
	    switch (evt) {
	      case JsonEventType::AGAIN:
		break;

	      case JsonEventType::INT_VALUE:
		builder.addValue(intPayload());
		break;

	      case JsonEventType::FLOAT_VALUE:
		builder.addValue(floatPayload());
		break;

	      case JsonEventType::STRING_VALUE:
		builder.addValue(stringPayload());
		break;

	      case JsonEventType::TRUE_VALUE:
		builder.addValue(true);
		break;

	      case JsonEventType::FALSE_VALUE:
		builder.addValue(false);
		break;

	      case JsonEventType::NULL_VALUE:
		builder.addNullValue();
		break;

	      case JsonEventType::BEGIN_ARRAY:
		builder.addValue(readArray());
		break;

	      case JsonEventType::BEGIN_OBJECT:
		builder.addValue(readObject());
		break;

	      default:
		// Should not happen
		throw pistis::exceptions::IllegalStateError(
		    "Unknown event type", PISTIS_EX_HERE
		);
	    }
	  }

	  return builder.done();
	}

	FlexibleEventStream& operator=(const FlexibleEventStream&) = delete;
	FlexibleEventStream& operator=(FlexibleEventStream&) = default;

      private:
	std::string streamName_;
	FlexibleStreamReader<Stream, CharEncoder, Allocator> reader_;
	PayloadFactory payloadFactory_;
	JsonString payload_;
	JsonEventOrigin origin_;
	State current_;
	std::vector<State (FlexibleEventStream::*)()> stateStack_;

	State parseInitialValue_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::parseInitialValue_,
			 JsonEventType::AGAIN);
	  }
	  return parseValue_(lookAhead.ch, nullptr,
			     &FlexibleEventStream::parseInitialValue_);
	}
	
	State parseFirstKey_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::parseFirstKey_,
			 JsonEventType::AGAIN);
	  } else if (lookAhead.ch == '}') {
	    return State(stateStack_.pop_back(), JsonEventType::END_OBJECT);
	  } else {
	    return parseKey_(lookAhead.ch, false);
	  }
	}
	
	State parseNextKey_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::parseNextKey_,
			 JsonEventType::AGAIN);
	  } else if (lookAhead.ch == '}') {
	    return State(stateStack_.pop_back(), JsonEventType::END_OBJECT);
	  } else if (lookAhead.ch != ',') {
	    error_(reader_.position(), "\",\" missing");
	  } else {
	    reader_.advance();
	    lookAhead = reader_.lookAhead();
	    if (lookAhead.again) {
	      return State(&FlexibleEventStream::restartNextKey_,
			   JsonEventType::AGAIN);
	    } else {
	      return parseKey_(lookAhead.ch, false);
	    }
	  }
	}

	State restartNextKey_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::restartNextKey_,
			 JsonEventType::AGAIN);
	  } else {
	    return parseKey_(lookAhead.ch, true);
	  }
	}
	
	State parseKey_(char lookAhead, bool resumed) {
	  if (lookAhead != '"') {
	    error_(reader_.position(), "'\"' missing");
	  } else {
	    try {
	      if (reader_.nextString(payload_, origin, stringStart, resumed)) {
		return State(&FlexibleEventStream::parseObjectValue_,
			     JsonEventType::FIELD_NAME);
	      } else {
		return State(&FlexibleEventStream::restartNextKey_,
			     JsonEventType::AGAIN);
	      }
	    } catch(const FlexibleStreamReader<Stream>::StringNotTerminated& e) {
	      error_(e.origin(), "Field name not terminated");
	    } catch(const InvalidJsonStringError& e) {
	      error_(origin_, e.details());
	    }
	  }
	}

	State parseObjectValue_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::parseObjectValue_,
			 JsonEventType::AGAIN);
	  } else if (lookAhead.ch != ':') {
	    error_(reader_.position(), "\":\" missing");
	  } else {
	    reader_.advance();
	    lookAhead = reader_.lookAhead();
	    if (lookAhead.again) {
	      return State(&FlexibleEventStream::restartObjectValue_,
			   JsonEventType::AGAIN);
	    }
	    return parseValue_(lookAhead.ch,
			       &FlexibleEventStream::parseNextKey_,
			       &FlexibleEventStream::restartObjectValue_);
	  }
	}

	State restartObjectValue_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::restartObjectValue_,
			 JsonEventType::AGAIN);
	  } else {
	    return parseValue_(lookAhead.ch,
			       &FlexibleEventStream::parseNextKey_,
			       &FlexibleEventStream::restartObjectValue_,
			       true);
	  }
	}
	
	State parseFirstArrayValue_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::parseFirstArrayValue_,
			 JsonEventType::AGAIN);
	  } else if (lookAhead.ch == ']') {
	    return State(stateStack_.pop_back(), JsonEventType::END_ARRAY);
	  } else {
	    return parseValue_(lookAhead.ch,
			       &FlexibleEventStream::parseNextArrayValue_,
			       &FlexibleEventStream::restartArrayValue_);
	  }
	}
	
	State parseNextArrayValue_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::parseNextArrayValue_,
			 JsonEventType::AGAIN);
	  } else if (lookAhead.ch == ']') {
	    return State(stateStack_.pop_back(), JsonEventType::END_ARRAY);
	  } else if (lookAhead.ch != ',') {
	    error_(reader_.position(), "\",\" expected");
	  } else {
	    reader_.advance();
	    lookAhead = reader_.lookAhead();
	    if (lookAhead.again) {
	      return State(&FlexibleEventStream::restartArrayValue_,
			   JsonEventType::AGAIN);
	    } else {
	      return parseValue_(lookAhead.ch,
				 &FlexibleEventStream::parseNextArrayValue_,
				 &FlexibleEventStream::restartArrayValue_);
	    }
	  }
	}

	State restartArrayValue_() {
	  JsonLookAhead lookAhead = reader_.lookAhead();
	  if (lookAhead.again) {
	    return State(&FlexibleEventStream::restartArrayValue_,
			 JsonEventType::AGAIN);
	  } else {
	    return parseValue_(lookAhead.ch,
			       &FlexibleEventStream::parseNextArrayValue_,
			       &FlexibleEventStream::restartArrayValue_,
			       true);
	  }
	}
	
	State parseValue_(char lookAhead,
			  State (FlexibleEventStream::*nextState)(),
			  State (FlexibleEventStream::*restartState)(),
			  bool resumed) {
	  if (lookAhead == '"') {
	    if (reader_.nextString(payload_, origin_, resumed))
	      return State(nextState, JsonEventType::STRING);
	    } else {
	      return State(restartState, JsonEventType::AGAIN);
	    }
	  } else if (std::isdigit(lookAhead) || (lookAhead == '-')) {
	    try {
	      JsonEventType eventType;
	      if (reader_.nextNumber(eventType, payload_, origin_, resumed)) {
		return State(nextState, eventType);
	      } else {
		return State(restartState, JsonEventType::AGAIN);
	      }
	    } catch(const IncorrectAlignmentError& e) {
	      error_(e.origin(), "Value expected");
	    }
	  } else {
	    switch (lookAhead) {
	      case '{':
		reader_.advance();
		stateStack_.push_back(nextState);
		return State(&FlexibleEventStream::parseFirstKey_,
			     JsonEventType::BEGIN_OBJECT);

	      case '[':
		origin_ = reader_.advance();
		stateStack_.push_back(nextState);
		return State(&FlexibleEventStream::parseFirstArrayValue_,
			     JsonEventType::BEGIN_ARRAY);

	      case 'T':
	      case 't':
		return parseWord_("true", 4, JsonEventType::TRUE_VALUE,
				  nextStart, restartState, resumed);

	      case 'F':
	      case 'f':
		return parseWord_("false", 5, JsonEventType::FALSE_VALUE,
				  nextStart, restartState, resumed);

	      case 'N':
	      case 'n':
		return parseWord_("null", 4, JsonEventType::NULL_VALUE,
				  nextState, restartState, resumed);

	      default:
		error_(reader_.position(), "Value expected");
	    }
	  }
	}

	State parseWord_(const char* word, size_t length,
			 JsonEventType eventType,
			 State (FlexibleEventStream::*nextState)(),
			 State (FlexibleEventStream::*restartState)(),
			 bool resumed) {
	  try {
	    if (reader_.recognizeWord(word, length, origin_, resumed)) {
	      return State(nextState, eventType);
	    } else {
	      return State(restartState, JsonEventType::AGAIN);
	    }
	  } catch(const IncorrectAlignmentError& e) {
	    error_(reader_.position(), "Value expected");
	  }
	}

	template <typename Builder, typename String,
		  typename ArrayBuilderFactory, typename ObjectBuilderFactory>
	void readObjectValue_(Builder& builder, const String& fieldName,
			      const ArrayBuilderFactory& createArrayBuilder,
			      const ObjectBuilderFactory& createObjectBuilder) {
	  JsonEventType evt = next();
	  while (evt == JsonEventType::AGAIN) {
	    evt = next();
	  }

	  switch (evt) {
	    case JsonEventType::INT_VALUE:
	      builder.setField(fieldName, intPayload());
	      break;
	      
	    case JsonEventType::FLOAT_VALUE:
	      builder.setField(fieldName, floatPayload());
	      break;
	      
	    case JsonEventType::STRING_VALUE:
	      builder.setField(fieldName, stringPayload());
	      break;
	      
	    case JsonEventType::TRUE_VALUE:
	      builder.setField(fieldName, true);
	      break;
	      
	    case JsonEventType::FALSE_VALUE:
	      builder.setField(fieldName, false);
	      break;
	      
	    case JsonEventType::NULL_VALUE:
	      builder.setFieldToNull(fieldName);
	      break;
	      
	    case JsonEventType::BEGIN_ARRAY:
	      builder.setField(fieldName, readArray(createArrayBuilder,
						    createObjectBuilder));
	      break;
		
	    case JsonEventType::BEGIN_OBJECT:
	      builder.setField(fieldName, readObject(createArrayBuilder,
						     createObjectBuilder));
	      break;
	      
	    default:
	      throw pistis::exceptions::IllegalStateError(
		  "Unknown event type", PISTIS_EX_HERE
	      );
	  }
	}

	[[noreturn]] void error_(const JsonEventOrigin& origin,
				 const std::string& details) {
	  std::ostringstream msg;
	  msg << "Error on line " << origin.line() << ", column "
	      << origin.column() << " (offset " << origin.offset()
	      << ")";
	  if (!streamName_.empty()) {
	    msg << " of " << streamName_;
	  }
	  msg << ": " << details;
	  throw JsonParseError(msg.str(), origin);
	}
      };

    }
  }
}

#endif

