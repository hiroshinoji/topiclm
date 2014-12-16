#ifndef _TOPICLM_UTIL_HPP_
#define _TOPICLM_UTIL_HPP_

#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>
#include <pficommon/data/intern.h>
#include <pficommon/data/string/utility.h>
#include "random_util.hpp"
#include "config.hpp"
#include "serialization.hpp"

namespace topiclm {

// inline std::vector<std::vector<std::vector<int> > >
// ReadDocs(const std::string& file_name,
//          const std::string& unk_type,
//          pfi::data::intern<std::string>& type2ids,
//          bool train) {
//   std::ifstream ifs(file_name.c_str());
//   const int unk_id = type2ids.key2id(unk_type);
//   const int eos_id = type2ids.key2id(kEosKey);
//   std::vector<std::vector<std::vector<int> > > docs;
//   std::vector<std::vector<int> > doc;
//   for (std::string line; getline(ifs, line); ) {
//     line = pfi::data::string::strip(line);
//     if (line.empty()) {
//       if (!doc.empty()) {
//         docs.push_back(doc);
//       }
//       doc.clear();
//       continue;
//     }
//     auto tokens = pfi::data::string::split(line, ' ');
//     std::vector<int> token_ids;

//     token_ids.push_back(eos_id);

//     for (auto& token : tokens) {
//       token = pfi::data::string::strip(token);
//       if (token.empty()) continue;
//       int token_id = type2ids.key2id(token, train);
//       if (token_id == -1) { // unknown word in test
//         token_id = unk_id;
//       }
//       token_ids.push_back(token_id);
//     }
//     token_ids.push_back(eos_id);
//     doc.push_back(token_ids);
//   }
//   if (!doc.empty()) {
//     docs.push_back(doc);
//   }
//   return docs;
// }

inline std::vector<std::string> split_strip(std::string str) {
  auto splitted = pfi::data::string::split(str, ' ');
  for (size_t i = 0; i < splitted.size(); ++i) {
    splitted[i] = pfi::data::string::strip(splitted[i]);
  }
  splitted.erase(std::remove(splitted.begin(), splitted.end(), ""), splitted.end());
  return splitted;
}

template <typename Key, typename Value>
void EraseAndShrink(boost::container::flat_map<Key, Value>& x, Key& k) {
  x.erase(k);
  // if (x.size() < x.capacity() / 4) {
  //   boost::container::flat_map<Key, Value> y;
  //   y.reserve(x.capacity() / 2);
  //   for (auto& datum : x) {
  //     y.insert(datum);
  //   }
  //   x.swap(y);
  // }
}
template <typename T, typename Iter>
void EraseAndShrink(std::vector<T>& x, Iter it) {
  x.erase(it);
  if (x.size() < x.capacity() / 4) {
    std::vector<T> y;
    y.reserve(x.capacity() / 2);
    std::copy(x.begin(), x.end(), std::back_inserter(y));
    x.swap(y);
  }
}

template<typename iterator, typename Key>
iterator pairVectorLowerBound(iterator low, iterator high, const Key& key) {
  while (low < high) {
    iterator mid = low + (high - low) / 2;
    const Key& midKey = (*mid).first;
    if (midKey < key) low = mid + 1;
    else if (midKey > key) high = mid;
    else break;
  }
  return low + (high - low) / 2;
}

inline std::string getWord(int id, const pfi::data::intern<std::string>& intern) {
  assert(intern.exist_id(id));
  return intern.id2key(id);
}

inline double logsumexp (double x, double y, int init)
{
	double vmin = std::min(x, y);
	double vmax = std::max(x, y);
	
	if (init) return y;
	if (x == y) return x + log(2);
	if (vmax > vmin + 50)
		return vmax;
	else
		return vmax + log(1 + exp(vmin - vmax));
}
inline double logsumexp(std::vector<double>::const_iterator b,
                        std::vector<double>::const_iterator e) {
  if (b != e) {
    double z = 0;
    for (auto it = b; it != e; ++it) {
      z = logsumexp(z, *it, it == b);
    }
    return z;
  } else {
    return 1;
  }
}
inline void normalize_logpdf(std::vector<double>::iterator b, std::vector<double>::iterator e) {
  double log_sum = logsumexp(b, e);
  for (; b != e; ++b) {
    *b = exp(*b - log_sum);
  }
}
inline double logsumexp(const std::vector<double>& a) {
  return logsumexp(a.begin(), a.end());
}
inline void normalize_logpdf(std::vector<double>& lpdf) {
  return normalize_logpdf(lpdf.begin(), lpdf.end());
}

inline double digamma(double x) {
  double result = 0, xx, xx2, xx4;
  assert(x > 0);
  for ( ; x < 7; ++x)
    result -= 1/x;
  x -= 1.0/2.0;
  xx = 1.0/x;
  xx2 = xx*xx;
  xx4 = xx2*xx2;
  result += log(x)+(1./24.)*xx2-(7.0/960.0)*xx4+(31.0/8064.0)*xx4*xx2-(127.0/30720.0)*xx4*xx4;
  return result;
}

};

#endif /* _TOPICLM_UTIL_HPP_ */
