/** @file Definitions of events reported by the streaming parser */

#include <ostream>
#include <stddef.h>

namespace pistis {
  namespace json {
    namespace streaming {

      template <typename InputStream, typename TypeFactory>
      class StreamingJsonParser;

      enum class EventType {
	BEGIN_OBJECT,
	END_OBJECT,
	BEGIN_ARRAY,
	END_ARRAY,
	STRING,
	INTEGER,
	JSON_FLOAT,
	BOOLEAN,
	JSON_NULL
      };

      std::ostream& operator<<(std::ostream& out, EventType evt);

      class EventOrigin {
      public:
	EventOrigin(size_t offset, size_t line, size_t column):
	  offset_(offset), line_(line), column_(column) {
	}

	size_t offset() const { return offset_; }
	size_t line() const { return line_; }
	size_t column() const { return column_; }

      private:
	size_t offset_;
	size_t line_;
	size_t column_;
      };

      inline std::ostream& operator<<(std::ostream& out,
				      const EventOrigin& origin) {
	return out << "offset: " << origin.offset() << ", line: "
		   << origin.line() << ", column: " << origin.column();
      }
	
      class Event {
      public:
	virtual ~Event() { }
	
	EventType type() const { return type_; }
	const EventOrigin& origin() const { return origin_; }
	
      protected:
	Event(EventType type, const EventOrigin& origin):
	  type_(type), origin_(origin) {
	}

      private:
	EventType type_;
	EventOrigin origin_;
      };

      template <typename InputStream, typename TypeFactory>
      class BeginObjectEvent : public Event {
      public:
	BeginObjectEvent(StreamingJsonParser<InputStream, TypeFactory>& parser,
			 const EventOrigin& origin):
	  Event(EventType::BEGIN_OBJECT, origin), parser_(parser) {
	}

	typename TypeFactory::JsonObjectType read() const {
	  return parser_->readObject_();
	}

      private:
	StreamingJsonParser<InputStream, TypeFactory> parser_;
      };

      class EndObjectEvent : public Event {
      public:
	EndObjectEvent(const EventOrigin& origin):
	  Event(EventType::END_OBJECT, origin) {
	}
      };

      template <typename InputStream, typename TypeFactory>
      class BeginArrayEvent : public Event {
      public:
	BeginArrayEvent(StreamingJsonParser<InputStream, TypeFactory>& parser,
			const EventOrigin& origin):
	  Event(EventType::BEGIN_ARRAY, origin), parser_(parser) {
	}

	typename TypeFactory::ArrayType read() const {
	  return parser_->readArray_();
	}

      private:
	StreamingJsonParser<InputStream, TypeFactory> parser_;
      };

      class EndArrayEvent : public Event {
      public:
	EndArrayEvent(const EventOrigin& origin):
	  Event(EventType::END_ARRAY, origin) {
	}
      };

      template <typename String>
      class StringEvent {

      };

      template <typename Integer>
      class IntegerEvent {
      };

      template <typename Float>
      class FloatEvent {

      };

      class BooleanEvent {

      };

      class NullEvent {

      };

      
    }
  }
}
