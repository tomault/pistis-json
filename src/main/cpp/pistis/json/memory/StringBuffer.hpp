#ifndef __PISTIS__JSON__MEMORY__STRINGBUFFER_HPP__
#define __PISTIS__JSON__MEMORY__STRINGBUFFER_HPP__

#include <pistis/json/JsonString.hpp>
#include <pistis/memory/AllocationUtils.hpp>
#include <memory>
#include <string.h>

namespace pistis {
  namespace json {
    namespace memory {

      template <typename Allocator = std::allocator<char> >
      class StringBuffer : Allocator {
      public:
	StringBuffer(size_t initialSize = 16, size_t maxSize = (size_t)-1,
		     const Allocator& allocator = Allocator()):
	    Allocator(allocator), initialSize_(initialSize), maxSize_(maxSize),
	    begin_(initalSize ? this->allocate(initialSize) : nullptr),
	    end(begin_ + initialSize) {
	}
	StringBuffer(const StringBuffer&) = delete;
	StringBuffer(StringBuffer&& other) nothrow:
	    Allocator(std::move(other)), initialSize_(other.initialSize()),
	    maxSize(other.maxSize()), begin_(other.begin()), end_(other.end()),
	    eos_(other.eos_) {
	  other.reset();
	}
	~StringBuffer() {
	  free_();
	}

	const Allocator& allocator() const { return (const Allocator&)*this; }
	size_t allocated() const { return eos_ - begin_; }
	size_t size() const { return end_ - begin_; }
	size_t initialSize() const { return initialSize_; }
	size_t maxSize() const { return maxSize_; }
	char* begin() const { return begin_; }
	char* end() const { return end_; }
	JsonString str() const { return JsonString(begin(), end()); }

	void write(char c) {
	  if (end_ == eos_) {
	    increaseAllocation_();
	  }
	  *end_++ = c;
	}
	
	void write(const char* begin, const char* end) {
	  if (begin < end) {
	    const size_t sz = end - begin;
	    while ((eos_ - end_) < sz) {
	      increaseAllocation_();
	    }
	    ::memcpy(end_, begin, sz);
	    end_ += sz;
	  }
	}
	
	void clear() {
	  end_ = begin_;
	}

	void reset() {
	  if (allocated() > initialSize()) {
	    free_();
	    begin_ = this->allocate(initialSize());
	    eos_ = begin_ + initialSize();
	  }
	  end_ = begin_;
	}

	StringBuffer& operator=(const StringBuffer& other) = delete;
	StringBuffer& operator=(StringBuffer&& other) {
	  if (begin() != other.begin()) {
	    free_();
	  }
	  return *this;
	}

      private:
	size_t initialSize_;
	size_t maxSize_;
	char* begin_;
	char* end_;
	char* eos_;

	void free_() noexcept {
	  if (begin_) {
	    this->deallocate(begin_, allocated());
	  }
	}

	void increaseSize_() {
	  std::tie(begin_, end_, eos_) =
	    pistis::memory::increaseAllocation((Allocator&)*this, initialSize(),
					       maxSize(), begin(), end(), eos_);
	}
	
      };
      
    }
  }
}
#endif
