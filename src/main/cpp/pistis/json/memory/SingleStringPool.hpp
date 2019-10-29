#ifndef __PISTIS__JSON__MEMORY__SINGLESTRINGPOOL_HPP__
#define __PISTIS__JSON__MEMORY__SINGLESTRINGPOOL_HPP__

#include <pistis/json/JsonString.hpp>
#include <algorithm>
#include <memory>
#include <string.h>

namespace pistis {
  namespace json {
    namespace memory {

      template <typename Allocator = std::allocator<char> >
      class SingleStringPool : Allocator {
      public:
	SingleStringPool(const Allocator& allocator):
	    SingleStringPool(16, 1024, allocator) {
	}
	SingleStringPool(uint32_t initialSize, const Allocator& allocator):
	    SingleStringPool(initialSize, std::min(initialSize, 1024),
			     allocator) {
	}
	SingleStringPool(uint32_t initialSize = 16,
			 uint32_t maxAllocated = 1024,
			 const Allocator& allocator = Allocator()):
	    Allocator(allocator), initialSize_(initialSize),
	    maxAllocated_(std::min(initialSize, maxAllocated)),
	    begin_(nullptr), end_(nullptr), eos_(nullptr) {
	}
	~SingleStringPool() { clear(); }
	SingleStringPool(const SingleStringPool&) = delete;
	SingleStringPool(SingleStringPool&& other):
	    Allocator(std::move(other)), initialSize_(other.initialSize()),
	    maxAllocated_(other.maxAllocated()),
	    begin_(other.begin_), end_(other.end_), eos_(other.eos_) {
	  other.begin_ = nullptr;
	  other.end_ = nullptr;
	  other.eos_ = nullptr;
	}

	const Allocator& allocator() const { return (const Allocator&)*this; }
	uint32_t initialSize() const { return initialSize_; }
	uint32_t maxAllocated() const { return maxAllocated_; }
	uint32_t allocated() const { return eos_ - begin_; }
	uint32_t size() const { return end_ - begin_; }

	JsonString get() const { return JsonString(begin_, end_); }

	void reset() {
	  if (allocated() > maxAllocated()) {
	    this->deallocate(begin_, allocated());
	    begin_ = this->allocate(initialSize());
	    eos_ = begin_ + initialSize();
	  }
	  end_ = begin_;
	}

	void append();

	void put(const JsonString& s) {
	  if ((s.size() > allocated()) ||
	      ((allocated() > maxAllocated()) &&
	       (s.size() <= maxAllocated()))) {
	    if (begin_) {
	      this->deallocate(begin_, (size_t)(eos_ - begin_));
	    }
	    begin_ = this->allocate(s.size());
	    eos_ = begin_ + s.size();
	  }
	  ::memcpy(begin_, s.begin(), s.size());
	  end_ = begin_ + s.size();
	}
	
	void clear() {
	  if (begin_) {
	    this->deallocate(begin_, (size_t)(eos_ - begin_));
	    begin_ = nullptr;
	    end_ = nullptr;
	    eos_ = nullptr;
	  }
	}
	
	SingleStringPool& operator=(const SingleStringPool&) = delete;

	SingleStringPool& operator=(SingleStringPool&& other) {
	  if (begin_ != other.begin_) {
	    clear();

	    Allocator::operator=(std::move(other));
	    begin_ = other.begin_;
	    end_ = other.end_;
	    eos_ = other.eos_;
	    other.begin_ = nullptr;
	    other.end_ = nullptr;
	    other.eos_ = nullptr;
	  }
	  return *this;
	}

      private:
	uint32_t maxSize_;
	char* begin_;
	char* end_;
	char* eos_;
      };
      
    }
  }
}
#endif
