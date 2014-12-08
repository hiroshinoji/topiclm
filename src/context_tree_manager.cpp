#include <iostream>
#include <unordered_set>
#include "context_tree_manager.hpp"
#include "restaurant_manager.hpp"
#include "lambda_manager.hpp"
#include "parameters.hpp"
#include "word.hpp"
#include "restaurant.hpp"
#include "random_util.hpp"
#include "log.hpp"
#include "document_manager.hpp"
#include "node_util.hpp"
#include "table_based_sampler.hpp"

using namespace std;

namespace hpy_lda {

LambdaManagerInterface* GetLambdaManager(LambdaType lambda_type, const Parameters& parameters) {
  if (lambda_type == kWood) {
    cerr << "non hierarchical lambda set" << endl;    
    return new LambdaManager(parameters.lambda_parameter(),
                             parameters.topic_parameter().num_topics,
                             parameters.ngram_order());
  } else if (lambda_type == kHierarchical) {
    cerr << "hierarchical lambda set" << endl;
    return new HierarchicalLambdaManager(parameters.lambda_parameter(),
                                         parameters.ngram_order());
  }
  throw "unsupported lambda_type";
}

ContextTreeManager::ContextTreeManager(LambdaType lambda_type,
                                       TreeType tree_type,
                                       const Parameters& parameters,
                                       int lexicon)
    : ct_(parameters.ngram_order()),
      lmanager_(GetLambdaManager(lambda_type, parameters)),
      parameters_(parameters),
      lexicon_(lexicon),
      num_topics_(parameters.topic_parameter().num_topics),
      zero_order_pred_(1.0 / lexicon),
      zero_order_pass_(parameters.hpy_parameter().prior_pass()
                       / (parameters.hpy_parameter().prior_pass()
                          + parameters.hpy_parameter().prior_stop())),
      ngram_order_(parameters.ngram_order()),
      node_path_(parameters.ngram_order(), nullptr),
      stop_prior_path_(parameters.ngram_order()) {
  if (tree_type == kGraphical) {
    cerr << "graphical model set" << endl;
    rmanager_.reset(
        new RestaurantManager(node_path_, *lmanager_, parameters_, zero_order_pred_));
  } else {
    cerr << "non graphical mode set" << endl;
    rmanager_.reset(
        new NonGraphicalRestaurantManager(node_path_, *lmanager_, parameters_, zero_order_pred_));
  }
  
  node_path_[0] = ct_.root();
}

ContextTreeManager::~ContextTreeManager() {}

int ContextTreeManager::WalkTreeNoCreate(const std::vector<int>& sent,
                                         int current_idx,
                                         int current_depth) {
  return ct_.WalkTreeNoCreate(sent, current_idx, current_depth, node_path_);
}
int ContextTreeManager::WalkTree(const std::vector<int>& sent,
                                 int current_idx,
                                 int current_depth,
                                 int target_depth) {
  return ct_.WalkTree(sent, current_idx, current_depth, target_depth, node_path_);
}

void ContextTreeManager::UpTreeFromLeaf(Node* leaf, int depth) {
  util::UpTreeFromLeaf(leaf, depth, node_path_);
}
void ContextTreeManager::EraseEmptyNodes(int current_depth, int target_depth) {
  assert(current_depth > target_depth);
  ct_.EraseEmptyNodes(current_depth, target_depth, node_path_);
}
void ContextTreeManager::CalcLambdaPath(int word_depth) {
  lmanager_->CalcLambdaPath(node_path_, word_depth);
}
void ContextTreeManager::CalcStopPriorPath(int word_depth, int max_depth) {
  double pass_prob = 1;
  double prior_stop = parameters_.hpy_parameter().prior_stop();
  double prior_pass = parameters_.hpy_parameter().prior_pass();
  //int max_depth = stop_prior_path_.size();
  max_depth = min(max_depth, (int)stop_prior_path_.size() - 1);
  double p_sum = 0;
  for (int i = 0; i <= max_depth; ++i) {
    double node_stop_prob = 0;
    if (i <= word_depth) {
      auto& restaurant = node_path_[i]->restaurant();
      node_stop_prob = (restaurant.stop_customers() + prior_stop)
          / (restaurant.stop_customers() + restaurant.pass_customers()
             + prior_stop + prior_pass);
    } else {
      node_stop_prob = 1 - zero_order_pass_;
    }
    stop_prior_path_[i] = pass_prob * node_stop_prob;
    pass_prob *= (1 - node_stop_prob);
    p_sum += stop_prior_path_[i];    
  }
  if (p_sum == 0) stop_prior_path_[max_depth] = 1;
  else {
    for (int i = 0; i <= max_depth; ++i) {
      stop_prior_path_[i] /= p_sum;
    }
  }
  for (size_t i = max_depth + 1; i < stop_prior_path_.size(); ++i) {
    stop_prior_path_[i] = 0;
  }
}
void ContextTreeManager::CalcTestStopPriorPath(int word_depth) {
  double pass_prob = 1;
  double prior_stop = parameters_.hpy_parameter().prior_stop();
  double prior_pass = parameters_.hpy_parameter().prior_pass();
  double p_sum = 0;
  for (int i = 0; i <= word_depth; ++i) {
    double node_stop_prob = 0;

    auto& restaurant = node_path_[i]->restaurant();
    node_stop_prob = (restaurant.stop_customers() + prior_stop)
        / (restaurant.stop_customers() + restaurant.pass_customers()
           + prior_stop + prior_pass);
    stop_prior_path_[i] = pass_prob * node_stop_prob;
    pass_prob *= (1 - node_stop_prob);
    p_sum += stop_prior_path_[i];
  }
  if (p_sum == 0) stop_prior_path_[word_depth] = 1;
  else {
    for (int i = 0; i <= word_depth; ++i) {
      stop_prior_path_[i] /= p_sum;
    }
  }
  for (size_t i = word_depth + 1; i < stop_prior_path_.size(); ++i) {
    stop_prior_path_[i] = 0;
  }
}
void ContextTreeManager::ResamplingTableLabels() {
  auto& depth2nodes = GetDepth2Nodes();
  int label_changed = 0;
  int sum_sampled = 0;
  for (size_t i = 0; i < depth2nodes.size(); ++i) {
    for (auto node : depth2nodes[i]) {
      UpTreeFromLeaf(node, i);
      
      auto& restaurant = node->restaurant();
      auto type2topics = restaurant.type2topics();
      for (auto& type2topic : type2topics) {
        int type = type2topic.first;
        auto& topics = type2topic.second;
        for (auto topic : topics) {
          if (topic == kGlobalFloorId) continue;
          int global_labeled_tables = restaurant.labeled_tables(topic, type, 0);
          int local_labeled_tables = restaurant.tables(topic, type) - global_labeled_tables;
          vector<int> is_global_vec;
          for (int j = 0; j < global_labeled_tables; ++j) {
            is_global_vec.push_back(1);
          }
          for (int j = 0; j < local_labeled_tables; ++j) {
            is_global_vec.push_back(0);
          }
          sum_sampled += is_global_vec.size();
          random_shuffle(is_global_vec.begin(), is_global_vec.end(), *random);
          for (int old_is_global : is_global_vec) {
            lmanager_->RemoveTable(node_path_, topic, i, old_is_global);
            if (old_is_global) {
              rmanager_->RemoveCustomerFromPath(type, kGlobalFloorId, i);
            } else {
              rmanager_->RemoveCustomerFromPath(type, topic, i - 1);
            }
            CalcLambdaPath(i);
            double lambda = lmanager_->lambda(topic, i);
            rmanager_->CalcGlobalPredictivePath(type, i);
            rmanager_->CalcLocalPredictivePath(type, topic, i - 1);

            double global_label_prob = rmanager_->predictive(i, 0);
            double local_label_prob =
                i == 0 ? zero_order_pred_ : rmanager_->predictive(i - 1, topic);
            global_label_prob *= (1 - lambda);
            local_label_prob *= lambda;

            bool is_global = random->NextBernoille(
                global_label_prob / (global_label_prob + local_label_prob));
            if (is_global != old_is_global) {
              topic_t old_label = old_is_global ? 0 : -1;
              topic_t new_label = old_is_global ? -1 : 0;
              restaurant.ChangeTableLabelAtRandom(topic, type, old_label, new_label);
              ++label_changed;
            }
            lmanager_->AddNewTable(node_path_, topic, i, is_global);
            if (is_global) {
              rmanager_->AddCustomerToPath(type, kGlobalFloorId, i);
            } else {
              rmanager_->AddCustomerToPath(type, topic, i - 1);
            }
          }
        }
      }
    }
  }
  LOG("lambda") << "resampling change rate: "
                << label_changed << " / " << sum_sampled << endl;
}

void ContextTreeManager::TableBasedResample() {
  if (table_based_sampler_) {
    table_based_sampler_->SampleAllTablesOnce();
  }
}

string ContextTreeManager::PrintDepth2Tables() {
  return lmanager_->PrintDepth2Tables(GetDepth2Nodes());
}

const std::vector<double>& ContextTreeManager::lambda_path() const {
  return lmanager_->lambda_path();
}
const std::vector<Node*>& ContextTreeManager::current_node_path() const {
  return node_path_;
}
const std::vector<std::pair<double, double> >& ContextTreeManager::cache_path() const {
  return rmanager_->cache_path();
}
void ContextTreeManager::set_table_based_sampler(TreeType tree_type, DocumentManager& dmanager) {
  if (tree_type == kGraphical) {
    table_based_sampler_.reset(new GraphicalTableBasedSampler(parameters_, node_path_, *this, *lmanager_, dmanager));
  } else {
    table_based_sampler_.reset(new NonGraphicalTableBasedSampler(parameters_, node_path_, *this, *lmanager_, dmanager));
  }
}

RestaurantManager& ContextTreeManager::rmanager() {
  return *rmanager_;
}
TableBasedSampler& ContextTreeManager::tsampler() {
  return *table_based_sampler_;
}
const LambdaManagerInterface& ContextTreeManager::lmanager() const {
  return *lmanager_;
}

} // hpy_lda
