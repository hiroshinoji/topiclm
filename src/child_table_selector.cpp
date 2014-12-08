#include <iostream>
#include "node.hpp"
#include "restaurant.hpp"
#include "child_table_selector.hpp"
#include "log.hpp"

using namespace std;

namespace hpy_lda {

// @TODO: It is lazy: ckeck for exceed of # of customers are a bit tired.
//        Currently I only check the # of tables which can easily be calculated.
SelectTableState ChildTableSelector::SelectTable(int type,
                                                 topic_t floor_id,
                                                 int c_consider,
                                                 bool init_depth,
                                                 MovingNode& mnode) {
  if (max_num_tables_ >= 0 && current_num_tables_ + c_consider > max_num_tables_) {
    selected_size_ = 0;
    return kExceedMax;
  }
  
  auto node = mnode.node;
  auto& restaurant = node->restaurant();
  auto& internal = restaurant.internal(type);
  auto& section = internal.section(floor_id);
  auto& observeds = section.observeds;

  ReserveBuffer(0);
  //table_pdf_[0] = observeds.size();
  if (init_depth) {
    c_consider -= mnode.observeds.size();
    assert(c_consider >= 0);
  }
  if (c_consider == 0) {
    selected_size_ = 0;
    return kOK;
  }
  //auto& children = node->children();
  
  size_t buffer_idx = observeds.size();
  if (init_depth) {
    buffer_idx = 0;
  } else {
    if (buffer_.size() < observeds.size()) buffer_.resize(observeds.size());
    for (size_t i = 0; i < observeds.size(); ++i) buffer_[i] = 0;
  }
  size_t child_idx = 1;

  if (!node->children().empty()) {
    auto& type2child_nodes = node->type2child_nodes();
    auto it = type2child_nodes.find(type);
    if (it != type2child_nodes.end()) {
      auto& child_cand_nodes = (*it).second;
      for (auto& child_node : child_cand_nodes) {
        auto& r = child_node->restaurant();
        if (int t = r.labeled_tables(floor_id, type, -1)) {
          ReserveBuffer(child_idx);
          if (buffer_.size() <= buffer_idx + t) buffer_.resize(buffer_idx + t + 1);
          for (int i = 0; i < t; ++i) {
            buffer_[buffer_idx] = child_idx;
            ++buffer_idx;
          }
          child_candis_[child_idx] = child_node;
          ++child_idx;
        }
      }
    }
  }
  
  diffs_.assign(child_idx, 0);
  random_shuffle(buffer_.begin(), buffer_.begin() + buffer_idx, *random);
  for (int i = 0; i < c_consider; ++i) {
    ++diffs_[buffer_[i]];
  }
  if (diffs_[0]) {
    assert(mnode.observeds.empty());
    if (buffer_.size() <= observeds.size()) buffer_.resize(buffer_.size() + 1);
    for (size_t i = 0; i < observeds.size(); ++i) buffer_[i] = i;
    random_shuffle(buffer_.begin(),
                   buffer_.begin() + observeds.size(),
                   *random);
    size_t original_size = mnode.observeds.size();
    mnode.observeds.resize(original_size + diffs_[0]);
    for (int i = 0; i < diffs_[0]; ++i) {
      mnode.observeds[original_size + i] = observeds[buffer_[i]];
    }
  }
  selected_size_ = 0;
  node2selected_tables_.resize(child_idx);

  for (size_t i = 1; i < child_idx; ++i) {
    if (int selected_num_tables = diffs_[i]) {
      auto& child_node = child_candis_[i];
      node2selected_tables_[selected_size_++] = {child_node, selected_num_tables};
    }
  }
  return kOK;
}

MovingNode ChildTableSelector::GetMovingNode(
    size_t idx, int type, int node_depth, topic_t floor_id) {
  auto& node = node2selected_tables_[idx].first;
  auto selected = node2selected_tables_[idx].second;
  auto& section = node->restaurant().internal(type).section(floor_id);
  auto label_it = pairVectorLowerBound(section.customer_histogram.begin(),
                                       section.customer_histogram.end(),
                                       -1);
  assert(label_it != section.customer_histogram.end() && (*label_it).first == -1);
  auto& histogram = (*label_it).second;
  size_t buffer_idx = 0;
  for (auto& bucket : histogram) {
    for (int j = 0; j < bucket.second; ++j) {
      if (buffer_.size() <= buffer_idx) buffer_.resize(buffer_idx + 1);
      buffer_[buffer_idx++] = bucket.first;
    }
  }
  assert(buffer_idx >= (size_t)selected);
  random_shuffle(buffer_.begin(), buffer_.begin() + buffer_idx, *random);

  MovingNode mnode = {node, node_depth};
  mnode.table_customers.resize(selected);
  for (int i = 0; i < selected; ++i) {
    mnode.table_customers[i] = buffer_[i];
  }
  return mnode;
}

} // hpy_lda
