#ifndef _HPY_LDA_HISTOGRAM_TABLE_RESTAURANT_HPP_
#define _HPY_LDA_HISTOGRAM_TABLE_RESTAURANT_HPP_

#include <vector>
#include "random_util.hpp"
#include "section.hpp"
#include "add_remove_result.hpp"
#include "util.hpp"

namespace hpy_lda {

class HistogramTableRestaurant {
 public:
  HistogramTableRestaurant() : num_global_customers_(0), num_local_customers_(0) {}
  AddRemoveResult AddCustomer(bool is_global,
                              double global_parent_probability,
                              double local_parent_probability,
                              double lambda,
                              double c) {
    int& num_customers = is_global ? num_global_customers_ : num_local_customers_;
    auto& histogram = is_global ? global_histogram_ : local_histogram_;
    ++num_customers;

    double global_base_probability = global_parent_probability * (1 - lambda);
    double local_base_probability = local_parent_probability * lambda;
    
    if (num_customers == 1) {
      bool parent_is_global =
          random->NextBernoille(global_base_probability /
                                (global_base_probability + local_base_probability));
      BinaryHistoKey key = {parent_is_global, 1};
      histogram.emplace_back(key, 1);
      return parent_is_global ? GlobalTableChanged : LocalTableChanged;
    }
    size_t num_buckets = histogram.size();
    if (table_probs_.size() <= num_buckets + 1) {
      table_probs_.resize(num_buckets + 2);
    }
    for (size_t i = 0; i < num_buckets; ++i) {
      auto& datum = histogram[i];
      table_probs_[i] = datum.first.sitting_customers * datum.second;
    }
    table_probs_[num_buckets] = c * global_base_probability;
    table_probs_[num_buckets + 1] = c * local_base_probability;
    
    size_t sample = random->SampleUnnormalizedPdfRef(table_probs_, num_buckets + 1);
    if (sample >= num_buckets) {
      bool parent_is_global = sample == num_buckets;
      BinaryHistoKey key = {parent_is_global, 1};
      auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), key);
      if (it != histogram.end() && (*it).first == key) {
        ++(*it).second;
      } else {
        histogram.insert(it, {key, 1});
      }
      return parent_is_global ? GlobalTableChanged : LocalTableChanged;
    } else {
      BinaryHistoKey new_key = histogram[sample].first;
      new_key.sitting_customers += 1;
      --histogram[sample].second;
      if (histogram[sample].second == 0) {
        EraseAndShrink(histogram, histogram.begin() + sample);
      }
      auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), new_key);
      if (it != histogram.end() && (*it).first == new_key) {
        ++(*it).second;
      } else {
        histogram.insert(it, {new_key, 1});
      }
      return TableUnchanged;
    }
  }
  double AddCustomerFractionary(
      bool is_global,
      double enter_customer,
      double parent_probability,
      double c) {
    if (parent_probability == 0) { 
      // this is special case: if p_parent == 0, new table would never be created
      return 0;
    }
    double& fractional_customers = is_global ?
        fractional_global_customers_ : fractional_local_customers_;
    int& num_customers = is_global ? num_global_customers_ : num_local_customers_;
    double x = c * parent_probability;
    double fractional_table = x / (num_customers + fractional_customers + x);
    fractional_customers += enter_customer;
    return fractional_table;
  }
  AddRemoveResult AddCustomerNewTable(bool is_global, double lambda) {
    int& num_customers = is_global ? num_global_customers_ : num_local_customers_;
    auto& histogram = is_global ? global_histogram_ : local_histogram_;
    ++num_customers;

    bool parent_is_global = random->NextBernoille(1 - lambda);

    BinaryHistoKey key = {parent_is_global, 1};
    
    auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), key);
    if (it != histogram.end() && (*it).first == key) {
      ++(*it).second;
    } else {
      histogram.insert(it, {key, 1});
    }
    return parent_is_global ? GlobalTableChanged : LocalTableChanged;
  }
  AddRemoveResult RemoveCustomer(bool is_global) {
    int& num_customers = is_global ? num_global_customers_ : num_local_customers_;
    auto& histogram = is_global ? global_histogram_ : local_histogram_;
    --num_customers;
    assert(histogram.size() > 0);
    
    size_t num_buckets = histogram.size();
    if (table_probs_.size() < num_buckets) {
      table_probs_.resize(num_buckets);
    }
    for (size_t i = 0; i < num_buckets; ++i) {
      auto& datum = histogram[i];
      table_probs_[i] = datum.first.sitting_customers * datum.second;
    }
    int sample = random->SampleUnnormalizedPdfRef(table_probs_, num_buckets - 1);

    auto& remove_key = histogram[sample].first;
    if (remove_key.sitting_customers == 1) {
      bool remove_is_global = remove_key.parent_is_global;
      --histogram[sample].second;
      if (histogram[sample].second == 0) {
        EraseAndShrink(histogram, histogram.begin() + sample);
      }
      return remove_is_global ? GlobalTableChanged : LocalTableChanged;
    } else {
      --histogram[sample].second;
      BinaryHistoKey new_key = {remove_key.parent_is_global, remove_key.sitting_customers - 1};
      if (histogram[sample].second == 0) {
        EraseAndShrink(histogram, histogram.begin() + sample);
      }
      auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), new_key);
      if (it != histogram.end() && (*it).first == new_key) {
        ++(*it).second;
      } else {
        histogram.insert(it, {new_key, 1});
      }
      return TableUnchanged;
    }
  }
  void ResetFractionalCount() {
    fractional_global_customers_ = 0;
    fractional_local_customers_ = 0;
  }
  
  int num_global_customers() const { return num_global_customers_; }
  int num_local_customers() const { return num_local_customers_; }
  int num_global_tables() const {
    int num_tables = 0;
    for (auto& datum : global_histogram_) num_tables += datum.second;
    return num_tables;
  }
  int num_local_tables() const {
    int num_tables = 0;
    for (auto& datum : local_histogram_) num_tables += datum.second;
    return num_tables;
  }
  double fractional_global_customers() const { return fractional_global_customers_; }
  double fractional_local_customers() const { return fractional_local_customers_; }

  double logjoint(double gamma) const {
    double log_denom = 0;
    double log_newtable = 0;
    double log_oldtable = 0;
    
    int sum_customers = num_global_customers_ + num_local_customers_;
    if (sum_customers == 0) return 0;

    log_denom = lgamma(gamma) - lgamma(sum_customers + gamma);
    
    int t_global = num_global_tables();
    int t_local = num_local_tables();
    log_newtable += log(gamma) * t_global;
    log_newtable += log(gamma) * t_local;

    for (size_t i = 0; i < global_histogram_.size(); ++i) {
      int c_wl = global_histogram_[i].first.sitting_customers;
      int t_wl = global_histogram_[i].second;
      if (c_wl > 1) {
        for (int t = 0; t < t_wl; ++t) {
          log_oldtable += lgamma(c_wl);
        }
      }
    }
    return log_denom + log_newtable + log_oldtable;
  }
 private:
  int num_global_customers_;
  int num_local_customers_;

  double fractional_global_customers_;
  double fractional_local_customers_;
  
  std::vector<std::pair<BinaryHistoKey, int> > global_histogram_; // num_customers -> num_tables
  std::vector<std::pair<BinaryHistoKey, int> > local_histogram_;

  static std::vector<double> table_probs_;

  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(num_global_customers_)
        & MEMBER(num_local_customers_)
        & MEMBER(global_histogram_)
        & MEMBER(local_histogram_);
  }
};

} // hpy_lda

#endif /* _HPY_LDA_HISTOGRAM_TABLE_RESTAURANT_HPP_ */
