#ifndef _TOPICLM_CONTEXT_TREE_H_
#define _TOPICLM_CONTEXT_TREE_H_

#include <stack>
#include <queue>
#include <unordered_map>
#include <memory>
#include <pficommon/text/json.h>
#include "node.hpp"
#include "config.hpp"

namespace topiclm {

class ChildIterator;
class ContextTree;

class DfsNodeIterator;
class DfsPathIterator;

class IteratorState {
 public:
  IteratorState(Node* node_);
  bool MoreChildren();
  Node* pop();
 private:
  friend class DfsNodeIterator;  
  friend class DfsPathIterator;
  ChildMapType::iterator current_;
  Node* node_;
  int depth_;
};

class DfsNodeIterator {
 public:
  DfsNodeIterator(Node* root, const ContextTree& ct);
  Node* operator*();
  DfsNodeIterator& operator++();
  DfsNodeIterator operator++(int);
  bool HasMore();
 private:
  Node* root_;
  const ContextTree& ct_;
  //std::stack<WrappedNode> node_stack_;
  std::stack<IteratorState> iterator_state_stack_;
};

class DfsPathIterator {
 public:
  DfsPathIterator(Node* root, const ContextTree& ct);
  std::vector<Node*>& operator*();
  DfsPathIterator& operator++();
  DfsPathIterator operator++(int);
  bool HasMore();
 private:
  Node* root_;
  const ContextTree& ct_;
  std::vector<Node*> current_path_;
  std::stack<IteratorState> iterator_state_stack_;
};

class ContextTree {
 public:
  ContextTree(int ngram_order_, int eos_id);
  int WalkTree(const std::vector<int>& sent,
                int idx,
                int current_depth,
                int target_depth,
                std::vector<Node*>& node_path);
  int WalkTreeNoCreate(const std::vector<int>& sent,
                        int idx,
                        int current_depth,
                        std::vector<Node*>& node_path) const;
  void EraseEmptyNodes(int current_depth,
                       int target_depth,
                       const std::vector<Node*>& node_path);
  DfsNodeIterator GetDfsNodeIterator() const {
    return DfsNodeIterator(root_.get(), *this);
  }
  DfsPathIterator GetDfsPathIterator() const {
    return DfsPathIterator(root_.get(), *this);
  }
  int CountNodes() const;

  double CalcUnigramProbability(int type) const;

  Node* root() const { return root_.get(); }
  const std::vector<std::set<Node*> >& depth2nodes() const { return depth2nodes_; }
 private:
  std::unique_ptr<Node> root_;
  std::vector<std::set<Node*> > depth2nodes_;
  int eos_id_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    if (ar.is_read) {
      std::queue<Node*> node_queue;
      node_queue.push(root_.get());
      int i = 0;
      while (!node_queue.empty()) {
        Node* node = node_queue.front();
        node_queue.pop();
        ar & *node;
        depth2nodes_[node->depth()].insert(node);
        
        std::vector<int> child_types;
        ar & child_types;
        for (int type : child_types) {
          auto child_it = node->children().find(type);
          node_queue.push((*child_it).second.get());
        }
        ++i;
      }
      int j;
      ar & j;
      assert(i == j);
    } else {
      std::queue<Node*> node_queue;
      node_queue.push(root_.get());
      int i = 0;
      while (!node_queue.empty()) {
        Node* node = node_queue.front();
        node_queue.pop();
        ar & *node;
        std::vector<int> child_types;
        for (auto& child : node->children()) {
          child_types.push_back(child.first);
        }
        ar & child_types;
        for (int type : child_types) {
          auto child_it = node->children().find(type);
          node_queue.push((*child_it).second.get());
        }
        ++i;
      }
      ar & i;
    }
    ar & MEMBER(eos_id_);
  }
};

} // namespace topiclm

#endif /* _TOPICLM_CONTEXT_TREE_H_ */
