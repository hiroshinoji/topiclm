// Copyright (c)2008-2011, Preferred Infrastructure Inc.
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
// 
//     * Neither the name of Preferred Infrastructure nor the names of other
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef INCLUDE_GUARD_PFI_DATA_UNORDERED_MAP_H_
#define INCLUDE_GUARD_PFI_DATA_UNORDERED_MAP_H_

#include "../pfi-config.h"

#include <cmath>

#if HAVE_UNORDERED_MAP
#include <unordered_map>
#elif HAVE_TR1_UNORDERED_MAP
#include <tr1/unordered_map>
#else
#error "There is no unordered map implementation."
#endif

#include "functional_hash.h"

namespace pfi{
namespace data{

template <class Key, class Tp,
          class Hash = hash<Key>,
          class EqualKey = std::equal_to<Key>,
          class Alloc = std::allocator<std::pair<const Key, Tp> > >
class unordered_map :
    public unordered_namespace::unordered_map<Key, Tp, Hash, EqualKey, Alloc> {
  typedef unordered_namespace::unordered_map<Key, Tp, Hash, EqualKey, Alloc> Base;

public:
  explicit
  unordered_map(typename Base::size_type n = 10,
                const typename Base::hasher &hf = typename Base::hasher(),
                const typename Base::key_equal &eql = typename Base::key_equal(),
                const typename Base::allocator_type &a = typename Base::allocator_type())
    : Base(n, hf, eql, a) {}

  template <class InputIterator>
  unordered_map(InputIterator f, InputIterator l,
                typename Base::size_type n = 10,
                const typename Base::hasher &hf = typename Base::hasher(),
                const typename Base::key_equal &eql = typename Base::key_equal(),
                const typename Base::allocator_type &a = typename Base::allocator_type())
    : Base(f, l, n, hf, eql, a) {}

#if HAVE_TR1_UNORDERED_SET
  void reserve(typename Base::size_type n) {
    this->rehash(std::ceil(n / this->max_load_factor()));
  }
#endif
};

template <class Key, class Tp,
          class Hash = hash<Key>,
          class EqualKey = std::equal_to<Key>,
          class Alloc = std::allocator<std::pair<const Key, Tp> > >
class unordered_multimap :
    public unordered_namespace::unordered_multimap<Key, Tp, Hash, EqualKey, Alloc> {
  typedef unordered_namespace::unordered_multimap<Key, Tp, Hash, EqualKey, Alloc> Base;

public:
  explicit
  unordered_multimap(typename Base::size_type n = 10,
                     const typename Base::hasher &hf = typename Base::hasher(),
                     const typename Base::key_equal &eql = typename Base::key_equal(),
                     const typename Base::allocator_type &a = typename Base::allocator_type())
    : Base(n, hf, eql, a) {}

  template <class InputIterator>
  unordered_multimap(InputIterator f, InputIterator l,
                     typename Base::size_type n = 10,
                     const typename Base::hasher &hf = typename Base::hasher(),
                     const typename Base::key_equal &eql = typename Base::key_equal(),
                     const typename Base::allocator_type &a = typename Base::allocator_type())
    : Base(f, l, n, hf, eql, a) {}

#if HAVE_TR1_UNORDERED_SET
  void reserve(typename Base::size_type n) {
    this->rehash(std::ceil(n / this->max_load_factor()));
  }
#endif
};

} // data
} // pfi
#endif // #ifndef INCLUDE_GUARD_PFI_DATA_UNORDERED_MAP_H_
