#include <iostream>
#include "hpy_sampler.hpp"
#include "parameters.hpp"
#include "random_util.hpp"
#include "restaurant.hpp"
#include "context_tree.hpp"

using namespace std;

namespace hpy_lda {

//const size_t hyper_threathold = 4;

void UniformHpySampler::Update(
      const std::vector<std::set<Node*> >& depth2wnodes,
      HPYParameter& hpy_parameter) {
  //size_t len = min(hyper_threathold, depth2wnodes.size());
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    double concentration = hpy_parameter.concentration(i, 0);
    double discount = hpy_parameter.discount(i, 0);
    if (discount > 0) {
      discount = SampleDiscount(depth2wnodes.begin() + i,
                                depth2wnodes.begin() + i + 1,
                                concentration,
                                discount);
      if (discount < 1e-10) {
        discount = 1e-10; // do not set zero
      }
    }
    if (concentration > 0) {
      concentration = SampleConcentration(depth2wnodes.begin() + i,
                                          depth2wnodes.begin() + i + 1,
                                          concentration,
                                          discount);
      if (concentration < 1e-10) {
        concentration = 1e-10;
      }
    }
    for (int j = 0; j <= num_topics_; ++j) {
      hpy_parameter.set_discount(i, j, discount);
      hpy_parameter.set_concentration(i, j, concentration);
    }
  }
}
double UniformHpySampler::SampleConcentration(
    std::vector<std::set<Node*> >::const_iterator depth_node_begin,
    std::vector<std::set<Node*> >::const_iterator depth_node_end,
    double concentration,
    double discount) {
  double hpy_yi = 0;
  double hpy_logx = 0;
  for (auto it = depth_node_begin; it != depth_node_end; ++it) {
    auto& some_depth_nodes = (*it);
    for (auto node : some_depth_nodes) {
      auto& restaurant = node->restaurant();
      auto& floor2c_t = restaurant.floor2c_t();
      for (auto& bucket : floor2c_t) {
        auto sum_customers = bucket.second.first;
        auto sum_tables = bucket.second.second;
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            hpy_yi += random->NextBernoille(concentration / (concentration + discount * i));
          }
        }
        if (sum_customers > 1) {
          hpy_logx += std::log(random->NextBeta(concentration + 1, (double)sum_customers - 1));
        }
      }
    }
  }
  return random->NextGamma(1 + hpy_yi, 1 - hpy_logx);
}
double UniformHpySampler::SampleDiscount(
    std::vector<std::set<Node*> >::const_iterator depth_node_begin,
    std::vector<std::set<Node*> >::const_iterator depth_node_end,
    double concentration,
    double discount) {
  double hpy_yi_inv = 0;
  double hpy_zwkj_inv = 0;
  for (auto it = depth_node_begin; it != depth_node_end; ++it) {
    auto& some_depth_nodes = (*it);
    for (auto node : some_depth_nodes) {
      auto& restaurant = node->restaurant();
      auto& floor2c_t = restaurant.floor2c_t();
      for (auto& bucket : floor2c_t) {
        auto sum_tables = bucket.second.second;
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            hpy_yi_inv += 1 - random->NextBernoille(concentration / (concentration + discount * i));
          }
        }
        auto table_histogram = restaurant.floor_table_histogram(bucket.first);
        for (auto& bucket : table_histogram) {
          int t = bucket.second;
          for (int i = 0; i < t; ++i) {
            int cuwk = bucket.first;
            for (int j = 1; j < cuwk; ++j) {
              hpy_zwkj_inv += 1 - random->NextBernoille((double)(j - 1) / (j - discount));
            }
          }
        }
      }
    }
  }
  return random->NextBeta(1 + hpy_yi_inv, 1 + hpy_zwkj_inv);
}
double UniformHpySampler::SampleCacheConcentration(
    const vector<set<Node*> >& depth2wnodes,
    double concentration,
    double discount) {
  double hpy_yi = 0;
  double hpy_logx = 0;
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    auto& some_depth_nodes = depth2wnodes[i];
    for (auto node : some_depth_nodes) {
      auto& restaurant = node->restaurant();
      auto& cache2c_t = restaurant.cache2c_t();
      for (auto& bucket : cache2c_t) {
        auto sum_customers = bucket.second.first;
        auto sum_tables = bucket.second.second;
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            hpy_yi += random->NextBernoille(concentration / (concentration + discount * i));
          }
        }
        if (sum_customers > 1) {
          hpy_logx += std::log(random->NextBeta(concentration + 1, (double)sum_customers - 1));
        }
      }
    }
  }
  return random->NextGamma(1 + hpy_yi, 1 - hpy_logx);
}
double UniformHpySampler::SampleCacheDiscount(
    const vector<set<Node*> >& depth2wnodes,
    double concentration,
    double discount) {
  double hpy_yi_inv = 0;
  double hpy_zwkj_inv = 0;
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    auto& some_depth_nodes = depth2wnodes[i];

    for (auto node : some_depth_nodes) {
      auto& restaurant = node->restaurant();
    
      auto& cache2c_t = restaurant.cache2c_t();
      for (auto& bucket : cache2c_t) {
        auto sum_tables = bucket.second.second;
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            hpy_yi_inv += 1 - random->NextBernoille(concentration / (concentration + discount * i));
          }
        }
        auto table_histogram = restaurant.floor_table_histogram(bucket.first, true);
        for (auto& bucket : table_histogram) {
          int t = bucket.second;
          for (int i = 0; i < t; ++i) {
            int cuwk = bucket.first;
            for (int j = 1; j < cuwk; ++j) {
              hpy_zwkj_inv += 1 - random->NextBernoille((double)(j - 1) / (j - discount));
            }
          }
        }
      }
    }
  }
  return random->NextBeta(1 + hpy_yi_inv, 1 + hpy_zwkj_inv);
}

void NonUniformHpySampler::Update(
    const std::vector<std::set<Node*> >& depth2wnodes,
    HPYParameter& hpy_parameter) {
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    for (int j = 0; j < num_topics_ + 1; ++j) {
      double concentration = hpy_parameter.concentration(i, j);
      double discount = hpy_parameter.discount(i, j);
      if (discount > 0) {
        discount = SampleDiscount(depth2wnodes.begin() + i,
                                  depth2wnodes.begin() + i + 1,
                                  j,
                                  concentration,
                                  discount);
        if (discount < 1e-10) {
          discount = 1e-10; // do not set zero
        }
      }
      if (concentration > 0) {
        concentration = SampleConcentration(depth2wnodes.begin() + i,
                                            depth2wnodes.begin() + i + 1,
                                            j,
                                            concentration,
                                            discount);
        if (concentration < 1e-10) {
          concentration = 1e-10;
        }
      }
      hpy_parameter.set_discount(i, j, discount);
      hpy_parameter.set_concentration(i, j, concentration);
    }
  }
}
double NonUniformHpySampler::SampleConcentration(
    std::vector<std::set<Node*> >::const_iterator depth_node_begin,
    std::vector<std::set<Node*> >::const_iterator depth_node_end,
    int topic,
    double concentration,
    double discount) {
    double hpy_yi = 0;
  double hpy_logx = 0;
  for (auto it = depth_node_begin; it != depth_node_end; ++it) {
    auto& some_depth_nodes = (*it);
    for (auto node : some_depth_nodes) {
      auto& restaurant = node->restaurant();
      auto& floor2c_t = restaurant.floor2c_t();
      auto floor_it = floor2c_t.find(topic);
      if (floor_it != floor2c_t.end()) {
        auto& bucket = *floor_it;
        auto sum_customers = bucket.second.first;
        auto sum_tables = bucket.second.second;
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            hpy_yi += random->NextBernoille(concentration / (concentration + discount * i));
          }
        }
        if (sum_customers > 1) {
          hpy_logx += std::log(random->NextBeta(concentration + 1, (double)sum_customers - 1));
        }
      }
    }
  }
  return random->NextGamma(1 + hpy_yi, 1 - hpy_logx);
}
double NonUniformHpySampler::SampleDiscount(
    std::vector<std::set<Node*> >::const_iterator depth_node_begin,
    std::vector<std::set<Node*> >::const_iterator depth_node_end,
    int topic,
    double concentration,
    double discount) {
  double hpy_yi_inv = 0;
  double hpy_zwkj_inv = 0;
  for (auto it = depth_node_begin; it != depth_node_end; ++it) {
    auto& some_depth_nodes = (*it);
    for (auto node : some_depth_nodes) {
      auto& restaurant = node->restaurant();
      auto& floor2c_t = restaurant.floor2c_t();
      auto floor_it = floor2c_t.find(topic);
      if (floor_it != floor2c_t.end()) {
        auto& bucket = *floor_it;
        auto sum_tables = bucket.second.second;
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            hpy_yi_inv += 1 - random->NextBernoille(concentration / (concentration + discount * i));
          }
        }
        auto table_histogram = restaurant.floor_table_histogram(bucket.first);
        for (auto& bucket : table_histogram) {
          int t = bucket.second;
          for (int i = 0; i < t; ++i) {
            int cuwk = bucket.first;
            for (int j = 1; j < cuwk; ++j) {
              hpy_zwkj_inv += 1 - random->NextBernoille((double)(j - 1) / (j - discount));
            }
          }
        }
      }
    }
  }
  return random->NextBeta(1 + hpy_yi_inv, 1 + hpy_zwkj_inv);
}

} // hpy_lda
