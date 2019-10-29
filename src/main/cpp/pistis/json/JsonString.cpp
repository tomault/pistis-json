#include "JsonString.hpp"

using namespace pistis::json;

size_t JsonString::hash() const {
  size_t result = 0;
  for (auto c : s) {
    result = result * 31 + (unsigned char)c;
  }
  return result;
}

int JsonString::cmp(const JsonString& other) const {
  const size_t sz = size();
  const size_t otherSz = other.size();
  
  if (sz < otherSz) {
    int o = ::strncmp(begin(), other.begin(), sz);
    return o ? o : -1;
  } else {
    int o = ::strncmp(begin(), other.begin(), otherSz);
    return o ? o :  otherSz - sz;
  }
}

int JsonString::cmp(const char* other) const {
  const char* p = begin();
  const char* q = other.begin();
  while ((p != end()) && *q) {
    const int delta = (int)(*p) - (int)(*q);
    if (delta) {
      return delta;
    }
    ++p; ++q;
  }
  if (p != end()) {
    return 1;  // "other" is shorter, since p != end(), so *q == 0
  } else if (*q) {
    return -1; // "this" is shorter, since *q != 0, so p == end()
  } else {
    return 0;  // same length, since p == end() and *q == 0
  }
}

