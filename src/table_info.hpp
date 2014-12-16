#ifndef _TOPICLM_TABLE_INFO_HPP_
#define _TOPICLM_TABLE_INFO_HPP_

#include "config.hpp"

namespace topiclm {

class Word;

struct TableInfo {
  topic_t floor;
  int customers;
  topic_t label;
  int obs_size;
  std::vector<std::shared_ptr<Word> > observeds;

  // TableInfo(topic_t floor, int customers, topic_t label)
  //     : floor(floor), customers(customers), label(label) {}
};

} // topiclm

#endif /* _TOPICLM_TABLE_INFO_HPP_ */
