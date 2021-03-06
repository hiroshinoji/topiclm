#include "node_util.hpp"
#include "node.hpp"

namespace topiclm {
namespace util {

void UpTreeFromLeaf(Node* leaf, int depth, std::vector<Node*>& node_path) {
  for (Node* current = leaf; current != nullptr; --depth, current = current->parent()) {
    node_path[depth] = current;
  }
}

} // util
} // topiclm
