#include "table_based_sampler.hpp"
#include "context_tree_manager.hpp"
#include "document_manager.hpp"
#include "floor_sampler.hpp"
#include "section_table_seq.hpp"
#include "child_table_selector.hpp"
#include "restaurant_manager.hpp"
#include "log.hpp"

using namespace std;

namespace topiclm {

unordered_map<int, int> CountDoc2MoveCustomers(
    const vector<pair<size_t, vector<MovingNode> > >& moving_nodes) {
  unordered_map<int, int> doc2move_customers;
  for (size_t j = 0; j < moving_nodes.size(); ++j) {
    if (size_t size = moving_nodes[j].first) {
      for (size_t l = 0; l < size; ++l) {
        for (auto& word : moving_nodes[j].second[l].observeds) {
          ++doc2move_customers[word->doc_id];
        }
      }
    }
  }
  return doc2move_customers;
}
// void ChangeTopics(const vector<pair<size_t, vector<MovingNode> > >& moving_nodes,
//                   const TableInfo& table_info,
//                   int type,
//                   topic_t k,
//                   topic_t sample,
//                   double alpha_sample,
//                   DocumentManager& dmanager,
//                   bool consider_general) {
//   for (size_t j = 0; j < moving_nodes.size(); ++j) {
//     if (size_t size = moving_nodes[j].first) {
//       for (size_t l = 0; l < size; ++l) {
//         auto& mnode_jl = moving_nodes[j].second[l];
//         for (auto& word : mnode_jl.observeds) {
//           dmanager.DecrementTopicCount(word->doc_id, k, word->is_general);          
//           if (consider_general) {
//             word->is_general = sample == 0;
//           }
//           dmanager.IncrementTopicCount(word->doc_id,
//                                        sample,
//                                        alpha_sample,
//                                        word->is_general);
//           dmanager.set_topic(*word, sample);
//           word->node->restaurant().SeparateWordFromSection(k, type, word);
//           word->node->restaurant().CombineSectionToWord(sample, type, word);
//         }
//       }
//     }
//   }
//   auto& mnode = moving_nodes[0].second[0];
//   auto& restaurant = mnode.node->restaurant();
//   restaurant.ChangeTablesFloor(k, type, table_info.label, mnode.table_customers, sample);
//   for (size_t j = 1; j < moving_nodes.size(); ++j) {
//     if (size_t size = moving_nodes[j].first) {
//       for (size_t l = 0; l < size; ++l) {
//         auto& mnode_jl = moving_nodes[j].second[l];
//         mnode_jl.node->restaurant().ChangeTablesFloor(
//             k, type, -1, mnode_jl.table_customers, sample);
//       }
//     }
//   }
// }

TableBasedSampler::TableBasedSampler(
    const Parameters& parameters,
    const std::vector<Node*>& node_path,
    ContextTreeManager& ct_manager,
    LambdaManagerInterface& lmanager,
    DocumentManager& dmanager)
    : parameters_(parameters),
      node_path_(node_path),
      ct_manager_(ct_manager),
      lmanager_(lmanager),
      dmanager_(dmanager),
      
      section_table_seq_(new SectionTableSeq()),
      child_table_selector_(new ChildTableSelector()),
      
      max_t_in_block_(-1),
      max_c_in_block_(-1),
      moving_nodes_(parameters.ngram_order()) {
}
TableBasedSampler::~TableBasedSampler() {}

SelectTableState TableBasedSampler::CollectMovingNodesRecur(
    int depth, int depth_diff, topic_t floor_id, int type, int& t_in_block, int& c_in_block) {
  auto& mnodes = moving_nodes_[depth_diff];
  auto num_nodes = mnodes.first;
  assert((depth_diff == 0 && num_nodes == 1) || depth_diff > 0);
  size_t current_idx = 0;
  for (size_t i = 0; i < num_nodes; ++i) {
    auto& mnode = mnodes.second[i];
    auto node = mnode.node;
    auto& restaurant = node->restaurant();

    auto& table_customers = mnode.table_customers;
    int c = accumulate(table_customers.begin(), table_customers.end(), 0);
    assert(c > 0);
    int t = table_customers.size();

    floor_sampler_->TakeInSectionLL(restaurant.floor2c_t(), c, t, floor_id, depth);

    child_table_selector_->set_current_num_tables(t_in_block);
    child_table_selector_->set_current_num_customers(c_in_block);
    auto ret = child_table_selector_->SelectTable(type, floor_id, c, depth_diff == 0, mnode);
    if (ret == kExceedMax) {
      return kExceedMax;
    }
    if (auto selected_size = child_table_selector_->selected_size()) {
      if (depth_diff == 0) t_in_block += selected_size;
      // @TODO: I don't count the number of customers, which should be calculated inside the for loop below
      assert((size_t)depth_diff + 1 < moving_nodes_.size());
      moving_nodes_[depth_diff + 1].first += selected_size;
      auto& depth_mnodes = moving_nodes_[depth_diff + 1].second;
      
      if (depth_mnodes.size() <= (current_idx + selected_size)) {
        depth_mnodes.resize(current_idx + selected_size + 1);
      }
      for (size_t j = 0; j < selected_size; ++j, ++current_idx) {
        depth_mnodes[current_idx] = child_table_selector_->GetMovingNode(
            j, type, depth + 1, floor_id);
        int c = accumulate(depth_mnodes[current_idx].table_customers.begin(),
                           depth_mnodes[current_idx].table_customers.end(), 0);
        t_in_block += c;
        if (SuspendCollectTables(t_in_block, c_in_block)) return kExceedMax;
      }
    }
  }
  if ((size_t)depth_diff + 1 < moving_nodes_.size()
      && moving_nodes_[depth_diff + 1].first) {
    auto child_ret = CollectMovingNodesRecur(depth + 1, depth_diff + 1, floor_id, type, t_in_block, c_in_block);
    if (child_ret == kExceedMax) return kExceedMax;
  }
  return kOK;
}
void TableBasedSampler::ChangeTopics(const TableInfo& anchor,
                                     int type,
                                     topic_t k,
                                     topic_t sample) {
  for (size_t j = 0; j < moving_nodes_.size(); ++j) {
    if (size_t size = moving_nodes_[j].first) {
      for (size_t l = 0; l < size; ++l) {
        auto& mnode_jl = moving_nodes_[j].second[l];
        for (auto& word : mnode_jl.observeds) {
          dmanager_.DecrementTopicCount(word->doc_id, k, word->is_general);

          // TODO: originally, this code is surrounded by if-state checking whether
          // ConsiderGeneral flag is on or not, but it seems unnecessary because 
          // this flag is only valid for cHPYTM: in DHPYTM, prob of sampling 0 is 0.
          word->is_general = sample == 0;
          
          dmanager_.IncrementTopicCount(word->doc_id,
                                        sample,
                                        parameters_.topic_parameter().alpha[sample],
                                        word->is_general);
          dmanager_.set_topic(*word, sample);
          word->node->restaurant().SeparateWordFromSection(k, type, word);
          word->node->restaurant().CombineSectionToWord(sample, type, word);
        }
      }
    }
  }
  auto& mnode = moving_nodes_[0].second[0];
  auto& restaurant = mnode.node->restaurant();
  restaurant.ChangeTablesFloor(k, type, anchor.label, mnode.table_customers, sample);
  for (size_t j = 1; j < moving_nodes_.size(); ++j) {
    if (size_t size = moving_nodes_[j].first) {
      for (size_t l = 0; l < size; ++l) {
        auto& mnode_jl = moving_nodes_[j].second[l];
        mnode_jl.node->restaurant().ChangeTablesFloor(
            k, type, -1, mnode_jl.table_customers, sample);
      }
    }
  }
}

void TableBasedSampler::SampleAllTablesOnce() {
  vector<int> sample_counts(parameters_.topic_parameter().num_topics + 1);
  vector<int> skipping_floor_flags(parameters_.topic_parameter().num_topics + 1, 0);

  int num_all_tables = 0;
  int num_skipped_tables = 0;

  auto& depth2nodes = ct_manager_.GetDepth2Nodes();
  int num_nodes = -1;
  int l = 0;
  for (auto& nodes : depth2nodes) num_nodes += nodes.size();
  for (size_t i = include_root_ ? 0 : 1; i < depth2nodes.size(); ++i) {
    for (auto node : depth2nodes[i]) {
      if (++l % 100 == 0) {
      cerr << "\t\t\t" << setw(6) << ++l << "/" << num_nodes << "\r";
      }
      auto& restaurant = node->restaurant();
      auto& type2internals = restaurant.type2internal();
      for (auto& type2internal : type2internals) {
        int type = type2internal.first;

        ResetSectionTableSeq(restaurant, type);

        fill(skipping_floor_flags.begin(), skipping_floor_flags.end(), 0);
        for (size_t n = 0; n < section_table_seq_->size(); ++n) {
          ++num_all_tables;
          auto& table_info = (*section_table_seq_)[n];
          topic_t k = table_info.floor;
          
          if (skipping_floor_flags[k]) {
            ++num_skipped_tables;
            continue;
          }
          ct_manager_.UpTreeFromLeaf(node, i);
          floor_sampler_->ClearPdf();
          ResetMovingNodes();

          moving_nodes_[0] = {1, {{node, (int)i}}};
          auto& mnode = moving_nodes_[0].second[0];
          mnode.table_customers.assign(1, table_info.customers);
          mnode.observeds = table_info.observeds;

          int t_in_block = 1;
          int c_in_block = table_info.customers;
          if (CollectMovingNodesRecur(i, 0, k, type, t_in_block, c_in_block) == kExceedMax) {
            // this (k, w) pair creates too large block, so skip all tables
            skipping_floor_flags[k] = 1;
            ++num_skipped_tables;
            continue;
          }
          
          auto doc2move_customers = CountDoc2MoveCustomers(moving_nodes_);
          floor_sampler_->TakeInPrior(k, doc2move_customers, dmanager_.doc2topic_count());
          
          RemoveFirstCustomerAndConsiderLambda(table_info, i, k, type);

          TakeInParentPredictives(i, type);
          
          int sample = floor_sampler_->Sample(k);
          //cerr << sample << endl;
          if (sample != k) {
            sample_counts[sample]++;
            ChangeTopics(table_info, type, k, sample);
          }
          AddFirstCustomerAndConsiderLambda(table_info, sample, i, type, restaurant);
        }
      }
    }
  }
  // for (size_t i = 0; i < sample_counts.size(); ++i) {
  //   LOG("floor_sample") << i << ": " << sample_counts[i] << "\n";
  // }
  // LOG("floor_sample_skip") << num_skipped_tables << " / " << num_all_tables << endl;
  // LOG("floor_sample") << endl;
}
void TableBasedSampler::ResetCacheInFloorSampler() {
  floor_sampler_->ResetTopicPriorCache(parameters_.topic_parameter().alpha);
  floor_sampler_->ResetArrangementCache(parameters_.hpy_parameter().depth2discount(),
                                        parameters_.hpy_parameter().depth2concentration());
}

void TableBasedSampler::set_max_t_in_block(int max_t_in_block) {
  max_t_in_block_ = max_t_in_block;
  child_table_selector_->set_max_num_tables(max_t_in_block_);
}
void TableBasedSampler::set_max_c_in_block(int max_c_in_block) {
  max_c_in_block_ = max_c_in_block;
  child_table_selector_->set_max_num_customers(max_c_in_block_);
}

GraphicalTableBasedSampler::GraphicalTableBasedSampler(
    const Parameters& parameters,
    const vector<Node*>& node_path,
    ContextTreeManager& ct_manager,
    LambdaManagerInterface& lmanager,
    DocumentManager& dmanager)
    : TableBasedSampler(parameters, node_path, ct_manager, lmanager, dmanager) {
  floor_sampler_.reset(new FloorSampler(parameters.topic_parameter().num_topics, false));
  ResetCacheInFloorSampler();
}

void GraphicalTableBasedSampler::ResetSectionTableSeq(Restaurant& r, int type) {
  section_table_seq_->Reset(r, type, false);
}

void GraphicalTableBasedSampler::RemoveFirstCustomerAndConsiderLambda(
    const TableInfo& anchor, int depth, topic_t k, int type) {
  assert(k != 0);
  if (anchor.label == -1) {
    lmanager_.RemoveTable(node_path_, k, depth, false);
    ct_manager_.rmanager().RemoveCustomerFromPath(type, k, depth - 1);
  } else {
    assert(anchor.label == 0);
    lmanager_.RemoveTable(node_path_, k, depth, true);
    ct_manager_.rmanager().RemoveCustomerFromPath(type, 0, depth);
  }
}

void GraphicalTableBasedSampler::AddFirstCustomerAndConsiderLambda(
    const TableInfo& anchor, topic_t sample, int depth, int type, Restaurant& r){
  double lambda = lmanager_.lambda(sample, depth);
  double global_parent = ct_manager_.rmanager().predictive(depth, 0) * (1 - lambda);
  double local_parent = ct_manager_.rmanager().predictive(depth - 1, sample) * lambda;
  bool parent_is_global = random->NextBernoille(global_parent / (global_parent + local_parent));
  //auto add_result = restaurant.AddCustomerNewTable(sample, type, (local_parent) / (global_parent + local_parent));
  auto new_label = parent_is_global ? 0 : -1;
  if (anchor.label != new_label) {
    r.ChangeTableLabel(sample, type, anchor.customers, anchor.label, new_label);
  }
  if (parent_is_global) {
    lmanager_.AddNewTable(node_path_, sample, depth, true);
    ct_manager_.rmanager().AddCustomerToPath(type, 0, depth);
  } else {
    lmanager_.AddNewTable(node_path_, sample, depth, false);
    ct_manager_.rmanager().AddCustomerToPath(type, sample, depth - 1);
  }
}
void GraphicalTableBasedSampler::TakeInParentPredictives(int depth, int type) {
  ct_manager_.rmanager().CalcDepth2TopicPredictives(type, depth);
  ct_manager_.rmanager().CalcMixtureParentPredictive(depth);
  floor_sampler_->TakeInParentPredictive(ct_manager_.rmanager().depth_predictives(depth));
}

NonGraphicalTableBasedSampler::NonGraphicalTableBasedSampler(
    const Parameters& parameters,
    const vector<Node*>& node_path,
    ContextTreeManager& ct_manager,
    LambdaManagerInterface& lmanager,
    DocumentManager& dmanager)
    : TableBasedSampler(parameters, node_path, ct_manager, lmanager, dmanager) {
  floor_sampler_.reset(new FloorSampler(parameters.topic_parameter().num_topics, true));
  ResetCacheInFloorSampler();
}

void NonGraphicalTableBasedSampler::ResetSectionTableSeq(Restaurant& r, int type) {
  section_table_seq_->Reset(r, type, true);
}
void NonGraphicalTableBasedSampler::RemoveFirstCustomerAndConsiderLambda(
    const TableInfo& /*anchor*/, int depth, topic_t k, int type) {
  ct_manager_.rmanager().RemoveCustomerFromPath(type, k, depth - 1);
  for (size_t j = 0; j < moving_nodes_.size(); ++j) {
    if (size_t size = moving_nodes_[j].first) {
      for (size_t l = 0; l < size; ++l) {
        auto& mnode_jl = moving_nodes_[j].second[l];
        mnode_jl.node->restaurant().ResetFractionalCount();
        if (mnode_jl.observeds.empty()) continue;
        ct_manager_.UpTreeFromLeaf(mnode_jl.node, mnode_jl.depth);
        for (size_t t = 0; t < mnode_jl.observeds.size(); ++t) {
          lmanager_.RemoveTable(node_path_, k, mnode_jl.depth, k == 0);
        }
      }
    }
  }
  auto& node_path = ct_manager_.current_node_path();
  for (int j = depth - 1; j >= 0; --j) {
    node_path[j]->restaurant().ResetFractionalCount();
  }
  for (size_t j = 0; j < moving_nodes_.size(); ++j) {
    if (size_t size = moving_nodes_[j].first) {
      for (size_t l = 0; l < size; ++l) {
        auto& mnode_jl = moving_nodes_[j].second[l];
        if (mnode_jl.observeds.empty()) continue;
        ct_manager_.UpTreeFromLeaf(mnode_jl.node, mnode_jl.depth);
        for (size_t t = 0; t < mnode_jl.observeds.size(); ++t) {
          lmanager_.CalcLambdaPathFractionary(node_path_, mnode_jl.depth);
          double local_lambda = lmanager_.lambda_path()[mnode_jl.depth];
          double global_lambda = lmanager_.global_lambda_path()[mnode_jl.depth];
          floor_sampler_->TakeInLambda(global_lambda, local_lambda);
          lmanager_.AddNewTableFractionary(node_path_, mnode_jl.depth);
        }
      }
    }
  }
}
void NonGraphicalTableBasedSampler::AddFirstCustomerAndConsiderLambda(
    const TableInfo&, topic_t sample, int depth, int type, Restaurant&) {
  for (size_t j = 0; j < moving_nodes_.size(); ++j) {
    if (size_t size = moving_nodes_[j].first) {
      for (size_t l = 0; l < size; ++l) {
        auto& mnode_jl = moving_nodes_[j].second[l];
        if (mnode_jl.observeds.empty()) continue;
        ct_manager_.UpTreeFromLeaf(mnode_jl.node, mnode_jl.depth);
        for (size_t t = 0; t < mnode_jl.observeds.size(); ++t) {
          lmanager_.CalcLambdaPath(node_path_, mnode_jl.depth);
          lmanager_.AddNewTable(node_path_, sample, mnode_jl.depth, sample == 0);
        }
      }
    }
  }
  ct_manager_.rmanager().AddCustomerToPath(type, sample, depth - 1);
}

void NonGraphicalTableBasedSampler::TakeInParentPredictives(int depth, int type) {
  if (depth > 0) {
    ct_manager_.rmanager().CalcDepth2TopicPredictives(type, depth - 1);
    floor_sampler_->TakeInParentPredictive(ct_manager_.rmanager().depth_predictives(depth - 1));
  }
}

} // topiclm
