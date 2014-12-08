#ifndef _HPY_LDA_TABLE_INFO_HPP_
#define _HPY_LDA_TABLE_INFO_HPP_

#include "config.hpp"

namespace hpy_lda {

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

} // hpy_lda

#endif /* _HPY_LDA_TABLE_INFO_HPP_ */
