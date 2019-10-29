#ifndef __PISTIS__JSON__JSONSTRING_HPP__
#define __PISTIS__JSON__JSONSTRING_HPP__

#include <functional>
#include <ostream>
#include <string>

#define JSON_STRING_RELATIONAL_OP(OP, CMP) \
  bool OP(const JsonString& other) const { return CMP; } \
  bool OP(const std::string& other) const { return CMP; } \
  bool OP(const char* other) const { return CMP; } \
  template <int N> bool OP(const char other[N]) const { return CMP; } \
  template <int N> bool OP(char other[N]) const { return CMP; }

#define JSON_STRING_REV_RELATIONAL_OP(OP, CMP) \
  bool OP(const std::string& x, const JsonString& y) const { return CMP; }   \
  bool OP(const char* x, const JsonString& y) const { return CMP; }	     \
  template <int N> bool OP(const char x[N], const JsonString& y) {           \
    return CMP;                                                              \
  }                                                                          \
  template <int N> bool OP(char x[N], const JsonString& y) { return CMP; }

namespace pistis {
  namespace json {

    class JsonString {
    public:
      JsonString(): begin_(nullptr), end_(nullptr) { }
      JsonString(const char* begin, const char* end)
	  : begin_(begin), end_(end) {
      }

      size_t size() const { return end_ - begin_; }
      const char* begin() const { return begin_; }
      const char* end() const { return end_; }

      int cmp(const JsonString& other) const;
      int cmp(const char* other) const;
      
      int cmp(const std::string& other) const {
	return cmp(JsonString(other.c_str(), other.c_str() + other.size()));
      }

      template <int N>
      int cmp(const char other[N]) const {
	return cmp(JsonString(&other, &other[N]));
      }

      template <int N>
      int cmp(char other[N]) const {
	return cmp(JsonString(&other, &other[N]));
      }

      size_t hash() const;
      std::string toString() const { return std::string(begin(), end()); }

      JSON_STRING_RELATIONAL_OP(operator==, !cmp(other));
      JSON_STRING_RELATIONAL_OP(operator!=, (bool)cmp(other));
      JSON_STRING_RELATIONAL_OP(operator<, cmp(other) < 0);
      JSON_STRING_RELATIONAL_OP(operator>, cmp(other) > 0);
      JSON_STRING_RELATIONAL_OP(operator<=, cmp(other) <= 0);
      JSON_STRING_RELATIONAL_OP(operator>=, cmp(other) >= 0);
      
    private:
      const char* begin_;
      const char* end_;
    };

    std::ostream& operator<<(std::ostream& out, const JsonString& s) {
      out.write(s.begin(), s.size());
      return out;
    }

    JSON_STRING_REV_RELATIONAL_OP(operator==, !y.cmp(x));
    JSON_STRING_REV_RELATIONAL_OP(operator!=, (bool)y.cmp(x));
    JSON_STRING_REV_RELATIONAL_OP(operator<, y.cmp(x) < 0);
    JSON_STRING_REV_RELATIONAL_OP(operator>, y.cmp(x) > 0);
    JSON_STRING_REV_RELATIONAL_OP(operator<=, y.cmp(x) <= 0);
    JSON_STRING_REV_RELATIONAL_OP(operator>=, y.cmp(x) >= 0);
  }
}

namespace std {
  template<>
  struct hash<pistis::json::JsonString> {
    size_t operator()(const pistis::json::JsonString& s) const {
      return s.hash();
    }
  };  
}

#undef JSON_STRING_RELATIONAL_OP
#undef JSON_STRING_REV_RELATIONAL_OP
#endif
