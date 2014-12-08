/*
  Copyright (c) 2012, Hiroshi Noji
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the <organization> nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <copyright holder> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VECTOR_MAP_HPP_
#define VECTOR_MAP_HPP_

#include <cassert>
#include <algorithm>
#include <iterator>
#include <vector>
#include <memory>
#include <stdexcept>
#include "util.hpp"

namespace hpy_lda {


template<class Key, class T, typename size_type = unsigned int>
class VectorMap {
public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef std::pair<const Key, T> value_type;

  class VectorMapIterator;
  class VectorMapConstIterator;
  typedef VectorMapIterator iterator;
  typedef VectorMapConstIterator const_iterator;

  class VectorMapIterator : public std::iterator<std::bidirectional_iterator_tag, value_type> {
   public:
    VectorMapIterator(value_type* keyValues, value_type* pos)
        : keyValues(keyValues), pos(pos) {}

    value_type& operator*() const {
      return *pos;
    }

    const value_type* operator->() const {
      return &operator*();
    }

    VectorMapIterator operator++() {
      ++pos;
      return *this;
    }
  
    VectorMapIterator operator--() {
      --pos;
      return *this;
    }

    VectorMapIterator operator++(int) {
      VectorMapIterator tmp(*this);
      ++*this;
      return tmp;
    }

    VectorMapIterator operator--(int) {
      VectorMapIterator tmp(*this);
      --*this;
      return tmp;
    }

    bool operator==(const VectorMapIterator& it) const {
      return pos == it.pos;
    }

    bool operator!=(const VectorMapIterator& it) const {
      return pos != it.pos;
    }

   private:
    value_type* keyValues;
    value_type* pos;
    friend class VectorMap;
  };
  
  class VectorMapConstIterator : public std::iterator<std::bidirectional_iterator_tag, value_type> {
   public:
    VectorMapConstIterator(value_type* keyValues, value_type* pos)
        : keyValues(keyValues), pos(pos) {}
  
    const value_type& operator*() const {
      return *pos;
    }
  
    const value_type* operator->() const {
      return &operator*();
    }
  
    VectorMapConstIterator operator++() {
      ++pos;
      return *this;
    }
  
    VectorMapConstIterator operator--() {
      --pos;
      return *this;
    }

    VectorMapConstIterator operator--(int) {
      VectorMapConstIterator tmp(*this);
      --*this;
      return tmp;
    }
  
    VectorMapConstIterator operator++(int) {
      VectorMapConstIterator tmp(*this);
      ++*this;
      return tmp;
    }
  
    bool operator==(const VectorMapConstIterator& it) const {
      return pos == it.pos;
    }
  
    bool operator!=(const VectorMapConstIterator& it) const {
      return pos != it.pos;
    }
  
   private:
    value_type* keyValues;
    value_type* pos;
  };

  VectorMap() : keyValues(new value_type[1]), size_(0) {}
  VectorMap(const VectorMap& other) :
    keyValues(new value_type[other.capacity()]) {
    for (size_type i = 0; i < other.size_; ++i) {
      const_cast<Key&>(keyValues[i].first) = other.keyValues[i].first;
      this->keyValues[i].second = other.keyValues[i].second;
    }
    size_ = other.size_;
  }
  VectorMap& operator=(const VectorMap& other) {
    if (this != &other) {
      VectorMap& tmp(other);
      this->swap(tmp);
    }
    return *this;
  }
  void clear() {
    keyValues.reset(new value_type[1]);
    size_ = 0;
  }
  size_type count(const Key& x) const {
    if (find(x) != end()) {
      return 1;
    } else {
      return 0;
    }
  }
  size_type capacity() const {
    size_type powof2 = 1;
    while (powof2 < size_) powof2 <<= 1;
    return powof2;
  }
  bool empty() const {
    return size_ == 0;
  }
  size_type size() const {
    return size_;
  }
  const_iterator find(const Key& key) const {
    value_type* pos = pairVectorLowerBound(keyValues.get(), keyValues.get() + size_, key);

    if (pos != &keyValues[size_] && (*pos).first == key) {
      return const_iterator(keyValues.get(), pos);
    } else {
      return this->end();
    }
  }
  iterator find(const Key& key) {
    value_type* pos = pairVectorLowerBound(keyValues.get(), keyValues.get() + size_, key);

    if (pos != &keyValues[size_] && (*pos).first == key) {
      return iterator(keyValues.get(), pos);
    } else {
      return this->end();
    }
  }
  T& operator[](const Key& key) {
    value_type* pos = pairVectorLowerBound(keyValues.get(), keyValues.get() + size_, key);
    size_type offset = pos - keyValues.get();
    if (pos != &keyValues[size_] && (*pos).first == key) {
      return keyValues[offset].second;
    } else {
      return insert(offset, key, T());
    }
  }
  T& at(const Key& key) const {
    value_type* pos = pairVectorLowerBound(keyValues.get(), keyValues.get() + size_, key);
    if (pos != &keyValues[size_] && (*pos).first == key) {
      return keyValues[pos - keyValues.get()].second;
    } else {
      throw std::out_of_range("VectorMap::_M_range_check");
    }
  }
  iterator insert(const std::pair<Key, T>& x) {
    return insert(this->begin(), x);
  }
  iterator insert(iterator position, const std::pair<Key, T>& x) {
    value_type& positionValue = *(position.pos);
    if (positionValue.first == x.first) {
      positionValue.second = x.second;
      return position;
    } else {
      value_type* pos;
      if (x.first > positionValue.first) {
        pos = pairVectorLowerBound(position.pos, &keyValues[size_], x.first);
      } else {
        pos = pairVectorLowerBound(keyValues.get(), &keyValues[size_], x.first);
      }
      size_type offset = pos - keyValues.get();
      if (pos != &keyValues[size_] && (*pos).first == x.first) {
        (*pos).second = x.second;
      } else {
        insert(offset, x.first, x.second);
      }
      return iterator(keyValues.get(), keyValues.get() + offset);
    }
  }
  size_t erase(const Key& key) {
    if (empty()) return 0;
    value_type* pos = pairVectorLowerBound(keyValues.get(), keyValues.get() + size_, key);
    
    size_type offset = pos - keyValues.get();
    if (pos != &keyValues[size_] && (*pos).first == key) {
      for (size_type i = offset; i < size_ - 1; ++i) {
        const_cast<Key&>(keyValues[i].first) = std::move(keyValues[i+1].first);
        keyValues[i].second = std::move(keyValues[i+1].second);
      }
      --size_;
      if (size_ < capacity() / 4) {
        size_t new_capacity = capacity() / 2;
        value_type* newKeyValues = nullptr;
        try {
          newKeyValues = new value_type[new_capacity];
          for (size_type i = 0; i < size_; ++i) {
            std::swap(newKeyValues[i].second, keyValues[i].second);
            std::swap(const_cast<Key&>(newKeyValues[i].first), const_cast<Key&>(keyValues[i].first));
          }
        } catch (...) {
          delete[] newKeyValues;
          throw;
        }
        keyValues.reset(newKeyValues);
      }
      return 1;
    } else {
      return 0;
    }
  }
  iterator begin() {
    return iterator(keyValues.get(), keyValues.get());
  }
  iterator end() {
    return iterator(keyValues.get(), &keyValues[size_]);
  }
  const_iterator begin() const {
    return const_iterator(keyValues.get(), keyValues.get());
  }
  const_iterator end() const {
    return const_iterator(keyValues.get(), &keyValues[size_]);
  }
  void swap(VectorMap& rhs) {
    std::swap(size_, rhs.size_);
    std::swap(keyValues, rhs.keyValues);
  }
  
private:
  T& insert(size_t offset, const Key& key, const T& value) {
    ensure_capacity();

    for (size_type i = size_; i > offset; --i) {
      std::swap(const_cast<Key&>(keyValues[i].first), const_cast<Key&>(keyValues[i-1].first));
      std::swap(keyValues[i].second, keyValues[i-1].second);
    }

    const_cast<Key&>(keyValues[offset].first) = key;
    keyValues[offset].second = value;
    size_++;
    return keyValues[offset].second;
  }

  void ensure_capacity() {
    size_t c = capacity();

    if(c == size_) {
      size_t new_capacity = c * 2;
      value_type* newKeyValues = nullptr;
      
      try {
        newKeyValues = new value_type[new_capacity];
        for (size_type i = 0; i < size_; ++i) {
          std::swap(newKeyValues[i].second, this->keyValues[i].second);          
          std::swap(const_cast<Key&>(newKeyValues[i].first), const_cast<Key&>(this->keyValues[i].first));
        }
      } catch (...) {
        delete[] newKeyValues;
        throw;
      }

      this->keyValues.reset(newKeyValues);
      
    }
  }
  
  std::unique_ptr<value_type[]> keyValues;
  size_type size_;

};


template<class Key, class T, typename size_type = unsigned int>
inline void swap(VectorMap<Key, T, size_type>& lhs,
                 VectorMap<Key, T, size_type>& rhs) {
  lhs.swap(rhs);
}

} // hpy_lda

#endif /* VECTOR_MAP_HPP_ */
