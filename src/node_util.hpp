#ifndef _HPY_LDA_NODE_UTIL_HPP_
#define _HPY_LDA_NODE_UTIL_HPP_

#include <vector>

class Node;

namespace hpy_lda {
namespace util {

void UpTreeFromLeaf(Node* leaf, int depth, std::vector<Node*>& node_path);

} // util
} // hpy_lda

#endif /* _HPY_LDA_NODE_UTIL_HPP_ */
