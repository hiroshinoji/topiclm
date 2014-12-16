#include <vector>
#include <gtest/gtest.h>
#include "context_tree.hpp"
#include "restaurant.hpp"

using namespace topiclm;
using namespace std;

TEST(context_tree, trivial) {
  vector<int> seq = {-1, -1, 1, 2, 3, 4, 5, -1};

  ContextTree ct;
  auto root_only_path = ct.WalkTreeNoCreate(seq.begin() + 2);
  ASSERT_EQ((int)root_only_path.size(), 1);
}

TEST(context_tree, walk_tree_trivial) {
  vector<int> seq = {-1, -1, 1, 2, 3, 4, 5, -1};

  ContextTree ct;
  int ngram_order = 3;

  vector<witerator> words;
  for (auto it = seq.begin() + (ngram_order - 1); it != seq.end(); ++it) {
    words.push_back(it);
  }
  vector<Node*> node_path;
  ct.WalkTree(words[0], ngram_order - 1, node_path);
  ASSERT_EQ((int)node_path.size(), ngram_order);
}

TEST(context_tree, walk_tree_invaliant) {
  vector<int> seq = {-1, -1, 1, 2, 3, 4, 5, -1};

  ContextTree ct;
  int ngram_order = 3;

  vector<witerator> words;
  for (auto it = seq.begin() + (ngram_order - 1); it != seq.end(); ++it) {
    words.push_back(it);
  }
  
  vector<vector<Node*>> node_paths1(words.size());
  vector<vector<Node*>> node_paths2(words.size());
  vector<vector<Node*>> node_paths3(words.size());
  for (size_t i = 0; i < node_paths1.size(); ++i) {
    ct.WalkTree(words[i], ngram_order - 1, node_paths1[i]);
  }
  for (size_t i = 0; i < node_paths2.size(); ++i) {
    ct.WalkTree(words[i], ngram_order - 1, node_paths2[i]);
  }
  for (size_t i = 0; i < node_paths3.size(); ++i) {
    node_paths3[i] = ct.WalkTreeNoCreate(words[i], ngram_order - 1);
  }
  ASSERT_EQ(node_paths1, node_paths2);
  ASSERT_EQ(node_paths2, node_paths3);
}

TEST(context_tree, iterator) {
  vector<int> seq = {0, 0, 1, 2, 3, 4, 5};

  ContextTree ct;
  int ngram_order = 3;

  vector<witerator> words;
  for (auto it = seq.begin() + (ngram_order - 1); it != seq.end(); ++it) {
    vector<Node*> node_path;
    ct.WalkTree(it, ngram_order - 1, node_path);
  }
  auto it = ct.GetDfsNodeIterator();
  auto wnode = *it;
  ASSERT_EQ(wnode.depth, 0);
  ASSERT_TRUE(it.HasMore());

  int node_num = 1;
  ++it;
  for (; it.HasMore(); ++it) {
    auto wnode = *it;
    ASSERT_TRUE(wnode.depth > 0);
    node_num++;
  }
  ASSERT_EQ(node_num, 11);
}
