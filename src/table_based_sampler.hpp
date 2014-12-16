#ifndef _TOPICLM_TABLE_BASED_SAMPLER_HPP_
#define _TOPICLM_TABLE_BASED_SAMPLER_HPP_

#include "moving_node.hpp"
#include "select_table_state.hpp"
#include "config.hpp"

namespace topiclm {

class ContextTreeManager;
class FloorSampler;
class SectionTableSeq;
class ChildTableSelector;
class DocumentManager;
class Parameters;
class LambdaManagerInterface;
class Restaurant;
class TableInfo;

class TableBasedSampler {
 public:
  TableBasedSampler(const Parameters& parameters,
                    const std::vector<Node*>& node_path,
                    ContextTreeManager& ct_manager,
                    LambdaManagerInterface& lmanager,
                    DocumentManager& dmanager);
  virtual ~TableBasedSampler();
  SelectTableState CollectMovingNodesRecur(
      int depth, int depth_diff, topic_t floor_id, int type, int& t_in_block, int& c_in_block);
  
  void SampleAllTablesOnce();
  void ResetCacheInFloorSampler();
  
  void set_max_t_in_block(int max_t_in_block);
  void set_max_c_in_block(int max_c_in_block);
  void set_include_root(bool include_root) { include_root_ = include_root; }
 protected:
  virtual void ResetSectionTableSeq(Restaurant& r, int type) = 0;
  virtual void RemoveFirstCustomerAndConsiderLambda(
      const TableInfo& anchor, int depth, topic_t k, int type) = 0;
  virtual void AddFirstCustomerAndConsiderLambda(
      const TableInfo& anchor, topic_t sample, int depth, int type, Restaurant& r) = 0;
  virtual void TakeInParentPredictives(int depth, int type) = 0;
  void ChangeTopics(const TableInfo& anchor, int type, topic_t k, topic_t sample);
  
  void ResetMovingNodes() {
    for (size_t i = 0; i < moving_nodes_.size(); ++i) {
      moving_nodes_[i].first = 0;
    }
  }
  bool SuspendCollectTables(int t_in_block, int c_in_block) {
    if (max_t_in_block_ >= 0 && t_in_block > max_t_in_block_) return true;
    if (max_c_in_block_ >= 0 && c_in_block > max_c_in_block_) return true;
    return false;
  }
  
  const Parameters& parameters_;
  const std::vector<Node*>& node_path_;
  ContextTreeManager& ct_manager_;
  LambdaManagerInterface& lmanager_;
  DocumentManager& dmanager_;

  std::unique_ptr<FloorSampler> floor_sampler_;
  std::unique_ptr<SectionTableSeq> section_table_seq_;
  std::unique_ptr<ChildTableSelector> child_table_selector_;

  int max_t_in_block_;
  int max_c_in_block_;
  bool include_root_;

  std::vector<std::pair<size_t, std::vector<MovingNode> > > moving_nodes_;
  
};

class GraphicalTableBasedSampler : public TableBasedSampler {
 public:
  GraphicalTableBasedSampler(const Parameters& parameters,
                             const std::vector<Node*>& node_path,
                             ContextTreeManager& ct_manager,
                             LambdaManagerInterface& lmanager,
                             DocumentManager& dmanager);
 protected:
  virtual void ResetSectionTableSeq(Restaurant& r, int type);
  virtual void RemoveFirstCustomerAndConsiderLambda(
      const TableInfo& anchor, int depth, topic_t k, int type);
  virtual void AddFirstCustomerAndConsiderLambda(
      const TableInfo& anchor, topic_t sample, int depth, int type, Restaurant& r);
  virtual void TakeInParentPredictives(int depth, int type);
};

class NonGraphicalTableBasedSampler : public TableBasedSampler {
 public:
  NonGraphicalTableBasedSampler(const Parameters& parameters,
                                const std::vector<Node*>& node_path,
                                ContextTreeManager& ct_manager,
                                LambdaManagerInterface& lmanager,
                                DocumentManager& dmanager);
 protected:
  virtual void ResetSectionTableSeq(Restaurant& r, int type);
  virtual void RemoveFirstCustomerAndConsiderLambda(
      const TableInfo& anchor, int depth, topic_t k, int type);
  virtual void AddFirstCustomerAndConsiderLambda(
      const TableInfo& anchor, topic_t sample, int depth, int type, Restaurant& r);
  virtual void TakeInParentPredictives(int depth, int type);
};


};

#endif /* _TOPICLM_TABLE_BASED_SAMPLER_HPP_ */
