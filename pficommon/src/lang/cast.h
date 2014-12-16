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

#ifndef INCLUDE_GUARD_PFI_LANG_CAST_H_
#define INCLUDE_GUARD_PFI_LANG_CAST_H_

#include <string>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace pfi{
namespace lang{

namespace detail{

template <typename Target, typename Source, bool Same>
class lexical_cast{
public:
  static Target cast(const Source &arg){
    Target ret;
    std::stringstream ss;
    if (!(ss<<arg && ss>>ret))
      throw std::bad_cast();
    
    return ret;
  }
};

template <typename Target, typename Source>
class lexical_cast<Target, Source, true>{
public:
  static Target cast(const Source &arg){
    return arg;
  }  
};

template <typename Source>
class lexical_cast<std::string, Source, false>{
public:
  static std::string cast(const Source &arg){
    std::ostringstream ss;
    ss<<arg;
    return ss.str();
  }
};

template <typename Target>
class lexical_cast<Target, std::string, false>{
public:
  static Target cast(const std::string &arg){
    Target ret;
    std::istringstream ss(arg);
    if (!(ss>>ret))
      throw std::bad_cast();
    return ret;
  }
};

template <typename T1, typename T2>
struct is_same {
  static const bool value = false;
};

template <typename T>
struct is_same<T, T>{
  static const bool value = true;
};

} // detail

template<typename Target, typename Source>
Target lexical_cast(const Source &arg)
{
  return detail::lexical_cast<Target, Source, detail::is_same<Target, Source>::value>::cast(arg);
}

} // lang
} // pfi
#endif // #ifndef INCLUDE_GUARD_PFI_LANG_CAST_H_
