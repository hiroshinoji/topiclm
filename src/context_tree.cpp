#include <iostream>
#include "context_tree.hpp"
#include "restaurant.hpp"

using namespace pfi::text::json;
using namespace std;

namespace hpy_lda {

IteratorState::IteratorState(Node* node)
    : node_(node) {
  current_ = node_->begin();
}
bool IteratorState::MoreChildren() {
  return current_ != node_->end();
}
Node* IteratorState::pop() {
  Node* ret = (*current_).second.get();
  ++current_;
  return ret;
}

// class DfsNodeIterator
DfsNodeIterator::DfsNodeIterator(Node* root, const ContextTree& ct)
    : root_(root), ct_(ct) {
  Node* node = root_;
  IteratorState r(node);
  r.depth_ = 0;
  iterator_state_stack_.push(r);
}
Node* DfsNodeIterator::operator*() {
  return iterator_state_stack_.top().node_;
}
DfsNodeIterator& DfsNodeIterator::operator++() {
  if (iterator_state_stack_.empty()) return *this;
  while (!iterator_state_stack_.empty() &&
         !iterator_state_stack_.top().MoreChildren()) {
    iterator_state_stack_.pop();
  }
  if (iterator_state_stack_.empty()) {
    return *this;
  }
  int child_depth = iterator_state_stack_.top().depth_ + 1;

  Node* node = iterator_state_stack_.top().pop();

  IteratorState s(node);
  s.depth_ = child_depth;
  iterator_state_stack_.push(s);
  return *this;
}
DfsNodeIterator DfsNodeIterator::operator++(int) {
  DfsNodeIterator old(*this);
  operator++();
  return old;
}
bool DfsNodeIterator::HasMore() {
  return !iterator_state_stack_.empty();
}
// end class DfsNodeIterator

//class DfsPathIterator
DfsPathIterator::DfsPathIterator(Node* root, const ContextTree& ct)
    : root_(root), ct_(ct) {
  Node* node = root_;
  IteratorState r(node);
  r.depth_ = 0;
  iterator_state_stack_.push(r);
  current_path_.push_back(node);
  while (iterator_state_stack_.top().MoreChildren()) {
    int child_depth = iterator_state_stack_.top().depth_ + 1;
    Node* child = iterator_state_stack_.top().pop();
    IteratorState c(child);
    c.depth_ = child_depth;
    iterator_state_stack_.push(c);
    current_path_.push_back(c.node_);
  }
}
vector<Node*>& DfsPathIterator::operator*() {
  return current_path_;
}
DfsPathIterator& DfsPathIterator::operator++() {
  if (iterator_state_stack_.empty()) return *this;
  while (!iterator_state_stack_.empty() &&
         !iterator_state_stack_.top().MoreChildren()) {
    iterator_state_stack_.pop();
    current_path_.pop_back();
  }
  if (iterator_state_stack_.empty()) {
    return *this;
  }
  while (iterator_state_stack_.top().MoreChildren()) {
    int child_depth = iterator_state_stack_.top().depth_ + 1;
    Node* child = iterator_state_stack_.top().pop();
    IteratorState s(child);
    s.depth_ = child_depth;
    iterator_state_stack_.push(s);
    current_path_.push_back(s.node_);
  }
  return *this;
}
DfsPathIterator DfsPathIterator::operator++(int) {
  DfsPathIterator old(*this);
  operator++();
  return old;
}
bool DfsPathIterator::HasMore() {
  return !iterator_state_stack_.empty();
}
ContextTree::ContextTree(int ngram_order_)
    : root_(new Node(-1, nullptr)),
      depth2nodes_(ngram_order_) {
  depth2nodes_[0].insert(root_.get());
}

int GetNextType(const std::vector<int>& sent,
                int idx,
                int current_depth) {
  int prev_idx = idx + current_depth; // if current_depth = 0: prev_idx = idx, if 1: prev_idx = idx + 1
  //if (prev_idx == (int)sent.size()-1) return -1;
  return sent.at(prev_idx + 1);
}

int ContextTree::WalkTree(const std::vector<int>& sent,
                          int idx,
                          int current_depth,
                          int target_depth,
                          std::vector<Node*>& node_path) {
  auto current = node_path[current_depth];
  //int next_type = GetNextType(sent, idx, current_depth);

  for (; idx >= 0 && current_depth < target_depth; --idx, ++current_depth) {
    auto child = current->child(sent[idx]);
    if (child == nullptr) {
      child = current->set_child(sent[idx], current);
      //current->add_type2childtype(next_type, sent[idx]);
      depth2nodes_[current_depth + 1].insert(child);
    }
    current = child;
    node_path[current_depth + 1] = current;
  }
  return current_depth;
}

int ContextTree::WalkTreeNoCreate(const vector<int>& sent,
                                  int idx,
                                  int current_depth,
                                  vector<Node*>& node_path) const {
  auto current = node_path[current_depth];

  for (; idx >= 0; --idx, ++current_depth) {
    auto child = current->child(sent[idx]);
    if (child == nullptr) {
      return current_depth;
    }
    current = child;
    node_path[current_depth + 1] = current;
  }
  return current_depth;
}

void ContextTree::EraseEmptyNodes(int current_depth,
                                  int target_depth,
                                  const std::vector<Node*>& node_path) {
  for (; current_depth > target_depth; --current_depth) {
    auto& restaurant = node_path[current_depth]->restaurant();
    if (restaurant.Empty()) {
      depth2nodes_[current_depth].erase(node_path[current_depth]);
      node_path[current_depth - 1]->EraseChild(node_path[current_depth]->type());
    }
  }
}
int ContextTree::CountNodes() const {
  int cnt = 0;
  for (auto node_it = GetDfsNodeIterator(); node_it.HasMore(); ++node_it, ++cnt) ;
  return cnt;
}


};
