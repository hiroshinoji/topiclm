#ifndef _HPY_LDA_SLICE_LIKELIHOOD_HPP_
#define _HPY_LDA_SLICE_LIKELIHOOD_HPP_

#include <vector>
#include "node.hpp"
#include "slice-sampler.h"

namespace hpy_lda {

struct LogDiscountPosterior {
  LogDiscountPosterior(const std::vector<std::shared_ptr<WrappedNode> >& some_depth_nodes,
                       const std::vector<int>& floor_ids,
                       double alpha,
                       double beta_a = 1,
                       double beta_b = 1)
      : some_depth_nodes(some_depth_nodes),
        floor_ids(floor_ids),
        alpha(alpha),
        beta_a(beta_a),
        beta_b(beta_b)  {}
  double operator()(double discount) const {
    double x = 1e-100;
    for (auto& wnode : some_depth_nodes) {
      auto& restaurant = wnode->node.restaurant();
      for (int floor : floor_ids) {
        int sum_tables = restaurant.floor_sum_tables(floor);
        if (sum_tables > 1) {
          for (int i = 1; i < sum_tables; ++i) {
            x += std::log(alpha + discount * i);
          }
        }
        auto table_vector = restaurant.floor_table_vector(floor);
        for (int cuwk : table_vector) {
          if (cuwk > 1) {
            for (int i = 1; i < cuwk; ++i) {
              x += std::log(i - discount);
            }
          }
        }
      }
    }
    x += (beta_a - 1) * std::log(discount);
    x += (beta_b - 1) * std::log(1 - discount);
    return x;
  }
  const std::vector<std::shared_ptr<WrappedNode> >& some_depth_nodes;
  const std::vector<int>& floor_ids;
  double alpha;
  double beta_a;
  double beta_b;
};

struct LogConcentrationPosterior {
  LogConcentrationPosterior(const std::vector<std::shared_ptr<WrappedNode> >& some_depth_nodes,
                            const std::vector<int>& floor_ids,
                            double discount,
                            double gamma_a = 1,
                            double gamma_b = 1)
      : some_depth_nodes(some_depth_nodes),
        floor_ids(floor_ids),
        discount(discount),
        gamma_a(gamma_a),
        gamma_b(gamma_b) {}
  double operator()(double alpha) const {
    double x = 1e-100;
    for (auto& wnode : some_depth_nodes) {
      auto& restaurant = wnode->node.restaurant();
      for (int floor : floor_ids) {
        int sum_customers = restaurant.floor_sum_customers(floor);
        int sum_tables = restaurant.floor_sum_tables(floor);
        if (sum_tables > 1) {
          for (int i =1; i < sum_tables; ++i) {
            x += std::log(alpha + discount * i);
          }
        }
        if (sum_customers > 1) {
          x += lgamma(alpha + 1) - lgamma(alpha + sum_customers);
        }
      }
    }
    x += (gamma_a - 1) * std::log(alpha);
    x -= alpha / gamma_b;
    return x;
  }
  
  const std::vector<std::shared_ptr<WrappedNode> >& some_depth_nodes;
  const std::vector<int>& floor_ids;
  double discount;
  double gamma_a;
  double gamma_b;
};

struct LogDirichletPosterior {
  LogDirichletPosterior(
      const std::vector<std::vector<int>*>& grouped_statistics,
      int topic_num,
      double gamma_a,
      double gamma_b,
      int start_i = 0) 
      : grouped_statistics(grouped_statistics),
        topic_num(topic_num),
        gamma_a(gamma_a),
        gamma_b(gamma_b),
        start_i(start_i) {}
  double operator()(double alpha) const {
    double sum_alpha = alpha * topic_num;
    double x = 1e-100;
    for (auto& each_count_ptr : grouped_statistics) {
      auto& each_count = *each_count_ptr;
      int sum_count = std::accumulate(each_count.begin() + start_i, each_count.end(), 0);
      if (sum_count > 0) {
        x += lgamma(sum_alpha) - lgamma(sum_count + sum_alpha);
      }
      for (size_t i = start_i; i < each_count.size(); ++i) {
        if (each_count[i] > 0) {
          x += lgamma(each_count[i] + alpha) - lgamma(alpha);
        }
      }
    }
    x += (gamma_a - 1) * log(alpha);
    x -= alpha * gamma_b;
    return x;
  };
  const std::vector<std::vector<int>*>& grouped_statistics;
  int topic_num;
  double gamma_a;
  double gamma_b;
  int start_i;
};
struct LogAssymmentricDirichletPosterior {
  LogAssymmentricDirichletPosterior(
      const std::vector<std::vector<int>*>& grouped_statistics,
      const std::vector<double>& alphas,
      int sampling_idx,
      double gamma_a,
      double gamma_b,
      int start_i = 0) 
      : grouped_statistics(grouped_statistics),
        alphas(alphas),
        sampling_idx(sampling_idx),
        gamma_a(gamma_a),
        gamma_b(gamma_b),
        start_i(start_i) {}
  double operator()(double alpha) const {
    double sum_alpha
        = std::accumulate(alphas.begin(), alphas.end(), 0.0)
        - alphas.at(sampling_idx) + alpha;
    double x = 1e-100;
    for (auto& each_count_ptr : grouped_statistics) {
      auto& each_count = *each_count_ptr;
      int sum_count = std::accumulate(each_count.begin() + start_i, each_count.end(), 0);
      if (sum_count > 0) {
        x += lgamma(sum_alpha) - lgamma(sum_count + sum_alpha);
      }
      x += lgamma(each_count.at(sampling_idx) + alpha) - lgamma(alpha);
    }
    x += (gamma_a - 1) * log(alpha);
    x -= alpha * gamma_b;
    return x;
  };
  const std::vector<std::vector<int>*>& grouped_statistics;
  const std::vector<double>& alphas;
  int sampling_idx;
  double gamma_a;
  double gamma_b;
  int start_i;
};
struct LogAssymmetricLocalDirichletPosterior {
  LogAssymmetricLocalDirichletPosterior(
      const std::vector<std::vector<int>*>& grouped_statistics,
      double global_alpha,
      int topic_num,
      double gamma_a,
      double gamma_b)
      : grouped_statistics(grouped_statistics),
        global_alpha(global_alpha),
        topic_num(topic_num),
        gamma_a(gamma_a),
        gamma_b(gamma_b) {}
  double operator()(double alpha) const {
    double sum_alpha = global_alpha + alpha * topic_num;
    double x = 1e-100;
    for (auto& each_count_ptr : grouped_statistics) {
      auto& each_count = *each_count_ptr;
      int sum_count = std::accumulate(each_count.begin(), each_count.end(), 0);
      if (sum_count > 0) {
        x += lgamma(sum_alpha) - lgamma(sum_count + sum_alpha);
      }
      for (size_t i = 1; i < each_count.size(); ++i) {
        if (each_count[i] > 0) {
          x += lgamma(each_count[i] + alpha) - lgamma(alpha);
        }
      }
    }
    x += (gamma_a - 1) * log(alpha);
    x -= alpha * gamma_b;
    return x;
  }
  const std::vector<std::vector<int>*>& grouped_statistics;
  double global_alpha;
  int topic_num;
  double gamma_a;
  double gamma_b;
};

struct LogLambdaPosterior {
  LogLambdaPosterior(const std::vector<std::shared_ptr<WrappedNode> >& some_depth_nodes,
                     double gamma_a,
                     double gamma_b)
      : some_depth_nodes(some_depth_nodes),
        gamma_a(gamma_a),
        gamma_b(gamma_b) {}
  double operator()(double c) const {
    double x = 1e-100;
    for (auto& wnode : some_depth_nodes) {
      double parent_lambda = 0.5;
      if (wnode->parent != nullptr) {
        parent_lambda = wnode->parent->lambda;
      }
      auto& restaurant = wnode->node.restaurant();
      int local_tables = restaurant.local_labeled_tables();
      int global_tables = restaurant.global_labeled_tables();
      if (local_tables + global_tables > 0) {
        x += lgamma(c) - lgamma(local_tables + global_tables + c);
      }
      if (local_tables > 0) {
        x += lgamma(local_tables + parent_lambda * c) - lgamma(parent_lambda * c);
      }
      if (global_tables > 0) {
        x += lgamma(global_tables + (1 - parent_lambda) * c)
            - lgamma((1 - parent_lambda) * c);
      }
    }
    x += (gamma_a - 1) * log(c);
    x -= c + gamma_b;
    return x;
  }
  const std::vector<std::shared_ptr<WrappedNode> >& some_depth_nodes;
  double gamma_a;
  double gamma_b;
};


};



#endif /* _HPY_LDA_SLICE_LIKELIHOOD_HPP_ */
