/** @file FlexibleStreamingJsonParser
 *
 *  Interface and implementation of
 *  pistis::json::streaming::FlexibleStreamingJsonParser, a streaming JSON
 *  parser returning flexible payload types.
 */
#ifndef __PISTIS__JSON__STREAMING__FLEXIBLESTREAMINGJSONPARSER_HPP__
#define __PISTIS__JSON__STREAMING__FLEXIBLESTREAMINGJSONPARSER_HPP__

#include <pistis/json/streaming/FlexibleEventStream.hpp>

namespace pistis {
  namespace json {
    namespace streaming {

      template <typename PayloadFactory>
      class FlexibleStreamingJsonParser {
      public:
	typedef FlexibleEventStream<detail::FileStreamAdapter, PayloadFactory>
	        FileStreamType;
	typedef FlexibleEventStream<detail::FdStreamAdapter, PayloadFactory>
	        FdStreamType;
	typedef FlexibleEventStream<detail::StringStreamAdapter, PayloadFactory>
	        StringEventStream;
	
      public:
	FlexibleStreamingJsonParser(const PayloadFactory& payloadFactory,
				    uint32_t bufferSize)
	    : payloadFactory_(payloadFactory), bufferSize_(bufferSize) {
	}
	FlexibleStreamingJsonParser(const FlexibleStreamingJsonParser&)
	    = default;
	FlexibleStreamingJsonParser(FlexibleStreamingJsonParser&&) = default;

	template <typename Stream>
	FlexibleEventStream<Stream, PayloadFactory> parseStream(
	    const std::string& streamName, Stream&& stream
        ) {
	  return FlexibleEventStream<Stream, PayloadFactory>(
	      streamName, std::move(stream), payloadFactory_, bufferSize_
	  );
	}

	FileStreamType parseFile(const std::string& filename) {
	  return parseStream(filename, detail::FileStreamAdapter(filename));
	}

	FdStreamType parseFile(const std::string& name, int fd) {
	  return parseStream(name, detail::FdStreamAdapter(fd));
	}

	FdStreamType parseFile(int fd) {
	  return parseFile("", fd);
	}


	StringStreamType parseString(const std::string& name,
				     const std::string& text) {
	  return parseStream(name, detail::StringStreamAdapter(text));
	}

	StringStreamType parseString(const std::string& text) {
	  return parseString("", text);
	}

	FlexibleStreamingJsonParser& operator=(
	    const FlexibleStreamingJsonParser&
	) = default;

	FlexibleStreamingJsonParser& operator=(
	    FlexibleStreamingJsonParser&&
        ) = default;

      private:
	PayloadFactory payloadFactory_;
	size_t bufferFactory_;
      };
      
    }
  }
}
#endif
