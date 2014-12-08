#include "restaurant_manager.hpp"
#include "lambda_manager.hpp"
#include "node.hpp"
#include "parameters.hpp"

using namespace std;

namespace hpy_lda {

RestaurantManager::RestaurantManager(const std::vector<Node*>& node_path,
                                     LambdaManagerInterface& lmanager,
                                     const Parameters& parameters,
                                     double zero_order_pred)
    : node_path_(node_path),
      lmanager_(lmanager),
      hpy_parameter_(parameters.hpy_parameter()),
      depth2topic_predictives_(parameters.ngram_order(),
                               vector<double>(parameters.topic_parameter().num_topics + 1, 0)),
      cache_path_(parameters.ngram_order()),
      zero_order_predictives_(parameters.topic_parameter().num_topics + 1, zero_order_pred) {
  Restaurant::SetBufferSize(parameters.topic_parameter().num_topics + 1);
}
RestaurantManager::~RestaurantManager() {}

void RestaurantManager::AddCustomerToPath(int type, topic_t topic, int word_depth) {
  auto floor_id = topic;
  for (int i = word_depth; i >= 0; --i) {
    auto& restaurant = node_path_[i]->restaurant();
    auto parent_global_predictive = depth2topic_predictives_[i][0];
    auto parent_local_predictive =
        i == 0 ? zero_order_predictives_[topic] : depth2topic_predictives_[i - 1][topic];
    
    auto lambda = lmanager_.lambda(floor_id, i);
    if (floor_id == 0) lambda = 1.0;
    auto add_result = restaurant.AddCustomer(
        floor_id,
        type,
        parent_local_predictive,
        {parent_global_predictive},
        lambda,
        {1.0},
        hpy_parameter_.discount(i, floor_id),
        hpy_parameter_.concentration(i, floor_id));
    if (add_result.first == AddRemoveResult::TableUnchanged) {
      break;
    }
    if (add_result.first == AddRemoveResult::GlobalTableChanged) {
      assert(add_result.second == 0);
      lmanager_.AddNewTable(node_path_, floor_id, i, true);
      floor_id = kGlobalFloorId;
      ++i;
    } else {
      assert(add_result.second == -1);
      if (floor_id != kGlobalFloorId) {
        lmanager_.AddNewTable(node_path_, floor_id, i, false);
      }
    }
  }
}
void RestaurantManager::AddCustomerToCache(int type, topic_t topic, int word_depth, topic_t doc_id) {
  node_path_[word_depth]->restaurant().AddCustomerToCache(doc_id, type, topic, hpy_parameter_.cache_discount(word_depth));
}
pair<AddRemoveResult, topic_t> RestaurantManager::RemoveCustomerFromCache(int type, int word_depth, topic_t doc_id) {
  return node_path_[word_depth]->restaurant().RemoveCustomer(doc_id, type, true);
}
void RestaurantManager::AddCustomerToPathAtRandom(int type, topic_t topic, int word_depth) {
  auto floor_id = topic;
  auto lambda = lmanager_.root_lambda();
  for (int i = word_depth; i >= 0; --i) {
    auto& restaurant = node_path_[i]->restaurant();
    if (floor_id == 0) lambda = 1;
    auto add_result = restaurant.AddCustomerNewTable(floor_id, type, lambda);
    assert(add_result != AddRemoveResult::TableUnchanged);
    if (add_result == GlobalTableChanged) {
      assert(floor_id != kGlobalFloorId);
      lmanager_.AddNewTableAtRandom(node_path_, floor_id, i, true);
      floor_id = kGlobalFloorId;
      ++i;
    } else {
      if (floor_id != kGlobalFloorId) {
        lmanager_.AddNewTableAtRandom(node_path_, floor_id, i, false);
      }
    }
  }
}

void RestaurantManager::RemoveCustomerFromPath(int type, topic_t topic, int word_depth) {
  auto floor_id = topic;
  for (int i = word_depth; i >= 0; --i) {
    auto& restaurant = node_path_[i]->restaurant();
    auto remove_result = restaurant.RemoveCustomer(floor_id, type);

    if (remove_result.first == AddRemoveResult::TableUnchanged) {
      break;
    }
    if (remove_result.first == AddRemoveResult::GlobalTableChanged) {
      lmanager_.RemoveTable(node_path_, floor_id, i, true);
      floor_id = kGlobalFloorId;
      ++i;
    } else {
      if (floor_id != kGlobalFloorId) {
        lmanager_.RemoveTable(node_path_, floor_id, i, false);
      }
    }
  }
}

void RestaurantManager::AddStopPassedCustomers(int word_depth) const {
  for (int i = 0; i < word_depth; ++i) {
    ++node_path_[i]->restaurant().pass_customers();
  }
  ++node_path_[word_depth]->restaurant().stop_customers();
}
void RestaurantManager::RemoveStopPassedCustomers(int word_depth) const {
  for (int i = 0; i < word_depth; ++i) {
    --node_path_[i]->restaurant().pass_customers();
  }
  --node_path_[word_depth]->restaurant().stop_customers();
}

void RestaurantManager::CombineSectionToWord(int type,
                                             topic_t floor_id,
                                             int word_depth,
                                             const std::shared_ptr<Word>& word_ptr,
                                             bool cache) {
  assert(node_path_[word_depth] == word_ptr->node);
  node_path_[word_depth]->restaurant().CombineSectionToWord(floor_id, type, word_ptr, cache);
}
void RestaurantManager::SeparateWordFromSection(int type,
                                                topic_t floor_id,
                                                int word_depth,
                                                const std::shared_ptr<Word>& word_ptr,
                                                bool cache) {
  assert(node_path_[word_depth] == word_ptr->node);  
  node_path_[word_depth]->restaurant().SeparateWordFromSection(floor_id, type, word_ptr, cache);
}

void RestaurantManager::CalcGlobalPredictivePath(int type, int word_depth) {
  auto predictive = zero_order_predictives_[0];
  for (int i = 0; i <= word_depth; ++i) {
    predictive = node_path_[i]->restaurant().predictive_probability(
        kGlobalFloorId,
        type,
        predictive,
        hpy_parameter_.discount(i, kGlobalFloorId),
        hpy_parameter_.concentration(i, kGlobalFloorId));
    depth2topic_predictives_[i][0] = predictive;
  }
}
void RestaurantManager::CalcLocalPredictivePath(int type, topic_t topic, int word_depth) {
  assert(topic != kGlobalFloorId);
  auto local_predictive = zero_order_predictives_[topic];
  for (int i = 0; i <= word_depth; ++i) {
    auto lambda = lmanager_.lambda(topic, i);
    auto mixture_base = lambda * local_predictive
        + (1 - lambda) * depth2topic_predictives_[i][0];
    local_predictive = node_path_[i]->restaurant().predictive_probability(
        topic,
        type,
        mixture_base,
        hpy_parameter_.discount(i, topic),
        hpy_parameter_.concentration(i, topic));
    depth2topic_predictives_[i][topic] = local_predictive;
  }
}

void RestaurantManager::CalcDepth2TopicPredictives(int type, int word_depth, bool test) {
  lmanager_.CalcLambdaPath(node_path_, word_depth);
  auto parent_predictives = &zero_order_predictives_;
  for (int i = 0; i <= word_depth; ++i) {
    auto& restaurant = node_path_[i]->restaurant();
    restaurant.FillInPredictives(*parent_predictives,
                                 type,
                                 lmanager_,
                                 i,
                                 hpy_parameter_,
                                 depth2topic_predictives_[i]);
    parent_predictives = &depth2topic_predictives_[i];
  }
  if (test) {
    for (size_t i = word_depth + 1; i < depth2topic_predictives_.size(); ++i) {
      std::fill(depth2topic_predictives_[i].begin(), depth2topic_predictives_[i].end(), 0.0);
    }
  } else {
    for (size_t i = word_depth + 1; i < depth2topic_predictives_.size(); ++i) {
      depth2topic_predictives_[i][0] = depth2topic_predictives_[i - 1][0];
      for (size_t j = 1; j < depth2topic_predictives_[0].size(); ++j) {
        auto lambda = lmanager_.lambda(j, i);
        depth2topic_predictives_[i][j]
            = lambda * depth2topic_predictives_[i - 1][j]
            + (1 - lambda) * depth2topic_predictives_[i][0];
      }
    }
  }
}
void RestaurantManager::CalcMixtureParentPredictive(int word_depth) {
  auto& parent_predictives = word_depth == 0 ? zero_order_predictives_
      : depth2topic_predictives_[word_depth - 1];
  for (size_t k = 1; k < depth2topic_predictives_[0].size(); ++k) {
    auto lambda = lmanager_.lambda(k, word_depth);
    depth2topic_predictives_[word_depth][k] =
        lambda * parent_predictives[k]
        + (1 - lambda) * depth2topic_predictives_[word_depth][0];
  }
}
void RestaurantManager::CalcCachePath(int type, int word_depth, topic_t cache_id) {
  for (int i = 0; i <= word_depth; ++i) {
    cache_path_[i] = node_path_[i]->restaurant().cache_probability(cache_id, type, hpy_parameter_.cache_discount(i), hpy_parameter_.cache_concentration(i));
  }
  for (size_t i = word_depth + 1; i < cache_path_.size(); ++i) {
    cache_path_[i] = {0, 1};
  }
}

NonGraphicalRestaurantManager::NonGraphicalRestaurantManager(
    const std::vector<Node*>& node_path,
    LambdaManagerInterface& lmanager,
    const Parameters& parameters,
    double zero_order_pred)
    : RestaurantManager(node_path, lmanager, parameters, zero_order_pred) {}
NonGraphicalRestaurantManager::~NonGraphicalRestaurantManager() {}

void NonGraphicalRestaurantManager::AddCustomerToPath(int type, topic_t topic, int word_depth) {
  auto floor_id = topic;
  for (int i = word_depth; i >= 0; --i) {
    auto& restaurant = node_path_[i]->restaurant();
    double parent_local_predictive =
        i == 0 ? zero_order_predictives_[topic] : depth2topic_predictives_[i - 1][topic];

    auto add_result = restaurant.AddCustomer(
        floor_id,
        type,
        parent_local_predictive,
        {0.0},
        1.0,
        {1.0},
        hpy_parameter_.discount(i, floor_id),
        hpy_parameter_.concentration(i, floor_id));
    if (add_result.first == AddRemoveResult::TableUnchanged) {
      break;
    }
    assert(add_result.first == AddRemoveResult::LocalTableChanged);
  }
}

void NonGraphicalRestaurantManager::AddObservedCustomerToPath(int type, topic_t topic, int word_depth) {
  lmanager_.AddNewTable(node_path_, topic, word_depth, topic == 0);
  AddCustomerToPath(type, topic, word_depth);
}

void NonGraphicalRestaurantManager::AddCustomerToPathAtRandom(int type,
                                                              topic_t topic,
                                                              int word_depth) {
  lmanager_.AddNewTableAtRandom(node_path_, topic, word_depth, topic == 0);
  int floor_id = topic;
  for (int i = word_depth; i >= 0; --i) {
    auto& restaurant = node_path_[i]->restaurant();
    auto add_result = restaurant.AddCustomerNewTable(floor_id, type, 1.0);
    assert(add_result == AddRemoveResult::LocalTableChanged);
  }
}

void NonGraphicalRestaurantManager::RemoveCustomerFromPath(int type,
                                                           topic_t topic,
                                                           int word_depth) {
  int floor_id = topic;
  for (int i = word_depth; i >= 0; --i) {
    auto& restaurant = node_path_[i]->restaurant();
    if (restaurant.RemoveCustomer(floor_id, type).first
        == AddRemoveResult::TableUnchanged) {
      break;
    }
  }
}

void NonGraphicalRestaurantManager::RemoveObservedCustomerFromPath(int type,
                                                                   topic_t topic,
                                                                   int word_depth) {
  lmanager_.RemoveTable(node_path_, topic, word_depth, topic == 0);
  RemoveCustomerFromPath(type, topic, word_depth);
}
void NonGraphicalRestaurantManager::CalcLocalPredictivePath(int type, topic_t topic, int word_depth) {
  assert(topic != kGlobalFloorId);
  double predictive = zero_order_predictives_[0];
  for (int i = 0; i <= word_depth; ++i) {
    predictive = node_path_[i]->restaurant().predictive_probability(
        topic,
        type,
        predictive,
        hpy_parameter_.discount(i, topic),
        hpy_parameter_.concentration(i, topic));
    depth2topic_predictives_[i][topic] = predictive;
  }
}
void NonGraphicalRestaurantManager::CalcDepth2TopicPredictives(int type, int word_depth, bool test) {
  lmanager_.CalcLambdaPath(node_path_, word_depth);
  auto parent_predictives = &zero_order_predictives_;
  for (int i = 0; i <= word_depth; ++i) {
    auto& restaurant = node_path_[i]->restaurant();
    restaurant.FillInPredictivesNonGraphical(*parent_predictives,
                                             type,
                                             i,
                                             hpy_parameter_,
                                             depth2topic_predictives_[i]);
    parent_predictives = &depth2topic_predictives_[i];
  }
  if (test) {
    for (size_t i = word_depth + 1; i < depth2topic_predictives_.size(); ++i) {
      std::fill(depth2topic_predictives_[i].begin(), depth2topic_predictives_[i].end(), 0.0);
    }
  } else {
    for (size_t i = word_depth + 1; i < depth2topic_predictives_.size(); ++i) {
      depth2topic_predictives_[i].assign(depth2topic_predictives_[i - 1].begin(),
                                         depth2topic_predictives_[i - 1].end());
    }
  }
}

} // hpy_lda
