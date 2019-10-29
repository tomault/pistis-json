#ifndef __PISTIS__JSON__STREAMING__JSONLOOKAHEAD_HPP__
#define __PISTIS__JSON__STREAMING__JSONLOOKAHEAD_HPP__

namespace pistis {
  namespace json {
    namespace streaming {

      /** @brief Value returned by a stream reader when its lookAhead()
       *         method is called.
       *
       *  A JsonLookAhead instance contains two values: the next character
       *  and a flag indicating that no character is available, but one might
       *  be in the future.  If both values are zero, then the reader has
       *  reached the end of its stream.
       */
      struct JsonLookAhead {
	/** @brief Nonzero if no character is available currently, but one
	 *         might be in the future.
	 *
	 *  Note that this value will be zero if the reader has reached the
	 *  end of its stream.
	 **/
	uint8_t again;

	/** @brief Next character from the stream.  Zero if the end of the
	 *         stream has been reached or if the stream has no data
	 *         currently, but might in the future.
	 */
	uint8_t ch;

	constexpr JsonLookAhead(uint8_t c): again(0), ch(c) { }
	constexpr JsonLookAhead(uint8_t a, uint8_t c): again(a), ch(c) { }

	static constexpr const JsonLookAhead AGAIN = JsonLookAhead(1, 0);
	static constexpr const JsonLookAhead END_OF_STREAM =
	    JsonLookAhead(0, 0);
      };
      
    }
  }
}
#endif
