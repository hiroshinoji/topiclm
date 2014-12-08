#ifndef _HPY_LDA_CONTEXT_TREE_MANAGER_HPP_
#define _HPY_LDA_CONTEXT_TREE_MANAGER_HPP_

#include <vector>
#include <memory>
#include <pficommon/data/intern.h>
#include "context_tree.hpp"
#include "context_tree_analyzer.hpp"

namespace hpy_lda {

class LambdaManagerInterface;
class Node;
class Parameters;
class RestaurantManager;
class DocumentManager;
class TableBasedSampler;

class ContextTreeManager {
  friend class ContextTreeAnalyzer;
 public:
  ContextTreeManager(LambdaType lambda_type,
                     TreeType tree_type,
                     const Parameters& parameters,
                     int lexicon);
  virtual ~ContextTreeManager();

  int WalkTreeNoCreate(const std::vector<int>& sent,
                       int current_idx,
                       int current_depth);
  int WalkTree(const std::vector<int>& sent,
               int current_idx,
               int current_depth,
               int target_depth); // return new depth  
  void UpTreeFromLeaf(Node* leaf, int node_depth);
  void EraseEmptyNodes(int current_depth, int target_depth);

  void CalcLambdaPath(int word_depth);

  void CalcStopPriorPath(int word_depth, int max_depth);
  void CalcTestStopPriorPath(int word_depth);

  void ResamplingTableLabels();

  void TableBasedResample();

  std::string PrintDepth2Tables();

  const std::vector<double>& stop_prior_path() const { return stop_prior_path_; }
  const std::vector<double>& lambda_path() const;
  ContextTreeAnalyzer GetCTAnalyzer(const pfi::data::intern<std::string>& intern) {
    return ContextTreeAnalyzer(*this, intern);
  }
  const std::vector<std::set<Node*> >& GetDepth2Nodes() const { return ct_.depth2nodes(); }
  const std::vector<Node*>& current_node_path() const;
  const std::vector<std::pair<double, double> >& cache_path() const;

  void set_table_based_sampler(TreeType tree_type, DocumentManager& dmanager);

  RestaurantManager& rmanager();
  TableBasedSampler& tsampler();
  const LambdaManagerInterface& lmanager() const;
  
 private:
  ContextTree ct_;
  std::unique_ptr<LambdaManagerInterface> lmanager_;
  std::unique_ptr<RestaurantManager> rmanager_;
  std::unique_ptr<TableBasedSampler> table_based_sampler_;
  
  const Parameters& parameters_;
  const int lexicon_;
  const int num_topics_;
  const double zero_order_pred_;
  const double zero_order_pass_;
  const int ngram_order_;

  // buffer
  std::vector<Node*> node_path_;
  std::vector<double> stop_prior_path_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(ct_);
    if (!lmanager_->topic2global_tables().empty()) {
      ar & MEMBER(lmanager_->topic2global_tables())
          & MEMBER(lmanager_->topic2local_tables());
    }
  }
};

};

#endif /* _HPY_LDA_CONTEXT_TREE_MANAGER_HPP_ */
