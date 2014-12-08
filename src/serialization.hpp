#ifndef _HPY_LDA_SERIALIZATION_HPP_
#define _HPY_LDA_SERIALIZATION_HPP_

#include <unordered_map>
#include <pficommon/data/serialization.h>
#include <pficommon/data/serialization/base.h>
#include <pficommon/data/serialization/pair.h>
#include <google/sparse_hash_map>
#include <boost/container/flat_map.hpp>
#include "vector_map.hpp"

namespace pfi {
namespace data {
namespace serialization {

template <class Archive, class K, class V, class H, class P, class A>
void serialize(Archive &ar, std::unordered_map<K, V, H, P, A> &m)
{
  uint32_t size=static_cast<uint32_t>(m.size());
  ar & size;

  if (ar.is_read){
    m.clear();
    while(size--){
      std::pair<K,V> v;
      ar & v;
      m.insert(v);
    }
  }
  else{
    for (typename std::unordered_map<K,V,H,P,A>::iterator p=m.begin();
         p!=m.end();p++){
      std::pair<K,V> v(*p);
      ar & v;
    }
  }
}
template <class Archive, class K, class V, class H, class P>
void serialize(Archive &ar, boost::container::flat_map<K, V, H, P> &m)
{
  uint32_t size=static_cast<uint32_t>(m.size());
  ar & size;

  if (ar.is_read){
    m.clear();
    while(size--){
      std::pair<K,V> v;
      ar & v;
      m.insert(v);
    }
  }
  else{
    for (typename boost::container::flat_map<K,V,H,P>::iterator p=m.begin();
         p!=m.end();p++){
      std::pair<K,V> v(*p);
      ar & v;
    }
  }
}

template <class Archive, class K, class V, class H, class P, class A>
void serialize(Archive &ar, google::sparse_hash_map<K, V, H, P, A> &m)
{
  uint32_t size=static_cast<uint32_t>(m.size());
  ar & size;

  if (ar.is_read){
    m.clear();
    while(size--){
      std::pair<K,V> v;
      ar & v;
      m.insert(v);
    }
  }
  else{
    for (typename google::sparse_hash_map<K,V,H,P,A>::iterator p=m.begin();
         p!=m.end();p++){
      std::pair<K,V> v(*p);
      ar & v;
    }
  }
}
template <class Archive, class K, class V, class H>
void serialize(Archive &ar, hpy_lda::VectorMap<K, V, H> &m)
{
  uint32_t size=static_cast<uint32_t>(m.size());
  ar & size;

  if (ar.is_read){
    m.clear();
    while(size--){
      std::pair<K,V> v;
      ar & v;
      m.insert(v);
    }
  }
  else{
    for (typename hpy_lda::VectorMap<K, V, H>::iterator p=m.begin();
         p!=m.end();p++){
      std::pair<K,V> v(*p);
      ar & v;
    }
  }
}


} // serialization
} // data
} // pfi


#endif /* _HPY_LDA_SERIALIZATION_HPP_ */
