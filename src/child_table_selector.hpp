#ifndef _HPY_LDA_CHILD_TABLE_SELECTOR_HPP_
#define _HPY_LDA_CHILD_TABLE_SELECTOR_HPP_

#include <vector>
#include "moving_node.hpp"
#include "config.hpp"
#include "select_table_state.hpp"

namespace hpy_lda {

class Node;

class ChildTableSelector {
 public:
  SelectTableState SelectTable(int type,
                               topic_t floor_id,
                               int c_consider,
                               bool init_depth,
                               MovingNode& mnode);
  MovingNode GetMovingNode(size_t idx, int type, int node_depth, topic_t floor_id);
  
  size_t selected_size() const { return selected_size_; }

  void set_current_num_tables(int t) { current_num_tables_ = t; }
  void set_current_num_customers(int c) { current_num_customers_ = c; }
  void set_max_num_tables(int t) { max_num_tables_ = t; }
  void set_max_num_customers(int c) { max_num_customers_ = c; }
  
 private:
  void ReserveBuffer(size_t last_idx) {
    if (child_candis_.size() <= last_idx) {
      //table_pdf_.resize(last_idx + 1);
      child_candis_.resize(last_idx + 1);
      diffs_.resize(last_idx + 1);
    }
  }
  int current_num_tables_;
  int current_num_customers_;
  int max_num_tables_;
  int max_num_customers_;
  
  //std::vector<double> table_pdf_;
  std::vector<Node*> child_candis_;
  std::vector<int> diffs_;
  
  std::vector<int> buffer_;

  std::vector<std::pair<Node*, int> > node2selected_tables_;
  size_t selected_size_;
  
};

} // hpy_lda

#endif /* _HPY_LDA_CHILD_TABLE_SELECTOR_HPP_ */
