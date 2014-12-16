#ifndef _TOPICLM_NODE_HPP_
#define _TOPICLM_NODE_HPP_

#include <stack>
#include <unordered_map>
#include <memory>
#include <pficommon/text/json.h>
#include "restaurant.hpp"
#include "serialization.hpp"
#include "config.hpp"

namespace topiclm {

class Node {
 public:
  friend class ChildIterator;
  Node(int type, Node* parent) : type_(type), parent_(parent) {}
  ~Node() {}

  void EraseChild(int type) {
    DeleteRegisteredChildType(type);
    auto child_it = children_.find(type);
    children_.erase(child_it);
  }

  Node* parent() const {
    return parent_;
  }
  Node* child(int type) {
    auto child_it = children_.find(type);
    if (child_it != children_.end()) {
      return (*child_it).second.get();
    } else {
      return nullptr;
    }
  }
  int depth() const {
    int depth = 0;
    for (Node* current = parent_;
         current != nullptr;
         ++depth, current = current->parent_) ;
    return depth;
  }
  ChildMapType::iterator begin() { return children_.begin(); }
  ChildMapType::iterator end() { return children_.end(); }

  Node* set_child(int type, Node* parent) {
    children_[type].reset(new Node(type, parent));
    return children_[type].get();
  }
  void add_type2child(int type, Node* child) {
    auto& child_nodes = type2child_nodes_[type];
    auto it = std::lower_bound(child_nodes.begin(), child_nodes.end(), child);
    if (it != child_nodes.end() && (*it) == child) return;
    //if (it != childtypes.end()) assert((*it) != childtype);
    child_nodes.insert(it, child);
  }
  
  Restaurant& restaurant() { return restaurant_; }
  const ChildMapType& children() const {
    return children_;
  }
  const std::vector<Node*>& child_node_having(int type) const {
    auto it = type2child_nodes_.find(type);
    assert(it != type2child_nodes_.end());
    return (*it).second;
  }
  const boost::container::flat_map<int, std::vector<Node*> >& type2child_nodes() const {
    return type2child_nodes_;
  }
  
  int type() const { return type_; }
  
 private:
  void DeleteRegisteredChildType(int childtype) {
    Node* childptr = child(childtype);
    for (auto& kv : type2child_nodes_) {
      std::vector<Node*>& child_nodes = kv.second;
      auto it = std::lower_bound(child_nodes.begin(), child_nodes.end(), childptr);
      if (it != child_nodes.end() && (*it) == childptr) {
        EraseAndShrink(child_nodes, it);
      }
    }
  }
  
  Node (const Node&);
  Node& operator=(const Node&);
  
  Restaurant restaurant_;
  ChildMapType children_;
  // currently, this information is not serialized/desirialized because this is not used at prediction
  boost::container::flat_map<int, std::vector<Node*> > type2child_nodes_;
  
  int type_;
  Node* parent_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & restaurant_;
    std::vector<int> type_vec;
    if (ar.is_read) {
      ar & type_vec;
      for (int type : type_vec) {
        children_[type].reset(new Node(type, this));
      }
    } else {
      for (auto& child : children_) {
        type_vec.push_back(child.first);
      }
      ar & type_vec;
    }
  }
};

} // namespace topiclm

#endif /* _TOPICLM_NODE_HPP_ */
