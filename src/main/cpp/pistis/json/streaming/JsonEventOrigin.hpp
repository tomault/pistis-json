#ifndef __PISTIS__JSON__STREAMING__JSONEVENTORIGIN_HPP__
#define __PISTIS__JSON__STREAMING__JSONEVENTORIGIN_HPP__

namespace pistis {
  namespace json {
    namespace streaming {

      class JsonEventOrigin {
      public:
	JsonEventOrigin() { }
	JsonEventOrigin(uint32_t line, uint32_t column, uint64_t offset):
	    line_(line), column_(column), offset_(offset) {
	}

	uint32_t line() const { return line_; }
	uint32_t column() const { return column_; }
	uint64_t offset() const { return offset_; }
	
      private:
	uint32_t line_;
	uint32_t column_;
	uint64_t offset_;
      };

      inline std::ostream& operator<<(std::ostream& out,
				      const JsonEventOrigin& origin) {
	return out << "line " << origin.line() << ", column "
		   << origin.column() << " (offset " << origin.offset()
		   << ")";
      }
      
    }
  }
}
#endif

