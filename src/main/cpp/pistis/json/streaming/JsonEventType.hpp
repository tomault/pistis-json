#ifndef __PISTIS__JSON__STREAMING__JSONEVENTTYPE_HPP__
#define __PISTIS__JSON__STREAMING__JSONEVENTTYPE_HPP__

#include <ostream>

namespace pistis {
  namespace json {
    namespace streaming {

      enum class JsonEventType {
	/** @brief Reached the end of the stream.
	 *
	 *  This is the final event in the stream.  Naturally, it has no
	 *  payload.
	 */
	END,

	/** @brief Could not read enough data from the event stream to
	 *         construct a complete event.  Try again.
	 *
	 *  When the parser can not read enough data to construct the next
	 *  event, but has not reached the end of the input stream, the
	 *  parser returns this event, telling the application to try to
	 *  get the next event at a later time.  This condition occurs when
	 *  the input stream is non-blocking, has no more data to read but
	 *  may have data in the future.  It will not occur if the socket
	 *  blocks until it receives data.
	 */
	AGAIN,

	/** @brief Encountered the start of a JSON object
	 *
	 *  The BEGIN_OBJECT event has no payload.  The parser returns
	 *  the content of the object as future events in the stream
	 *  (an alternating sequence of FIELD_NAME and object value events).
	 */
	BEGIN_OBJECT,

	/** @brief Encountered the end of a JSON object
	 *
	 *  The END_OBJECT event has no payload.
	 */
	END_OBJECT,

	/** @brief Encountered the start of a JSON array
	 *
	 *  The BEGIN_ARRAY event has no payload.  The parser returns the
	 *  contents of the array as future events in the stream.
	 */
	BEGIN_ARRAY,

	/** @brief Encountered the end of a JSON array
	 *
	 *  The END_ARRAY event has no payload.
	 */
	END_ARRAY,

	/** Encountered the name of an object field
	 *
	 *  The FIELD_NAME event has a string payload.  The parser will
	 *  return this event only if it is reading the content of an object.
	 */
	FIELD_NAME,

	/** Encountered an integer value.
	 *
	 *  This event has an integer payload (naturally).
	 */
	INT_VALUE,

	/** Encountered a float value.
	 *
	 *  This event has a float payload (naturally).
	 */
	FLOAT_VALUE,

	/** Encountered a string value
	 *
	 *  This event has a string payload (naturally).
	 */
	STRING_VALUE,

	/** Encountered a boolean value equal to true.
	 *
	 *  This event has no payload.
	 */
	TRUE_VALUE,

	/** Encountered a boolean value equal to false.
	 *
	 *  This event has no payload.
	 */
	FALSE_VALUE,

	/** Encountered a null value.
	 *
	 *  This event has no payload.
	 */
	NULL_VALUE
      };

      std::ostream& operator<<(std::ostream& out, JsonEventType t);
      
    }
  }
}

#endif
