#ifndef _TOPICLM_MOVING_NODE_HPP_
#define _TOPICLM_MOVING_NODE_HPP_

#include <vector>
#include <memory>

namespace topiclm {

class Node;
class Word;

struct MovingNode {
  MovingNode() {}
  MovingNode(Node* node, int depth) : node(node), depth(depth) {}
  Node* node;
  int depth;
  std::vector<int> table_customers;
  std::vector<std::shared_ptr<Word> > observeds;
};
 

} // topiclm

#endif /* _TOPICLM_MOVING_NODE_HPP_ */
