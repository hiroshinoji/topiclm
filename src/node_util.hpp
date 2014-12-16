#ifndef _TOPICLM_NODE_UTIL_HPP_
#define _TOPICLM_NODE_UTIL_HPP_

#include <vector>

class Node;

namespace topiclm {
namespace util {

void UpTreeFromLeaf(Node* leaf, int depth, std::vector<Node*>& node_path);

} // util
} // topiclm

#endif /* _TOPICLM_NODE_UTIL_HPP_ */
