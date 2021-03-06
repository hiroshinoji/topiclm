#ifndef _TOPICLM_WORD_HPP_
#define _TOPICLM_WORD_HPP_

#include "config.hpp"

namespace topiclm {

class Node;

struct Word {
  Word(int depth, int doc_id, int sent_idx, int token_idx)
      : depth(depth), doc_id(doc_id), sent_idx(sent_idx), token_idx(token_idx), is_general(false) {}

  int depth;
  int doc_id;
  int sent_idx;
  int token_idx;
  bool is_general;
  Node* node;
};

} // topiclm

#endif /* _TOPICLM_WORD_HPP_ */
