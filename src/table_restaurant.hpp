#ifndef _HPY_LDA_TABLE_RESTAURANT_HPP_
#define _HPY_LDA_TABLE_RESTAURANT_HPP_

#include <vector>
#include "random_util.hpp"
#include "serialization.hpp"

namespace hpy_lda {

class TableRestaurant {
 public:
  TableRestaurant() : num_global_customers_(0), num_local_customers_(0) {}
  bool AddCustomer(bool is_global,
                   double c,
                   double parent_probability) {
    int& num_customers = is_global ? num_global_customers_ : num_local_customers_;
    auto& tables = is_global ? global_tables_ : local_tables_;
    ++num_customers;
    if (num_customers == 1) {
      tables.push_back(1);
      return true;
    }
    if (table_probs_.size() <= tables.size()) {
      table_probs_.resize(tables.size() + 1);
    }
    for (size_t i = 0; i < tables.size(); ++i) table_probs_[i] = tables[i];
    table_probs_[tables.size()] = c * parent_probability;
  
    size_t table = random->SampleUnnormalizedPdfRef(table_probs_, tables.size());
    if (table == tables.size()) {
      tables.push_back(1);
      return true;
    } else {
      ++tables[table];
      return false;
    }
  }

  bool RemoveCustomer(bool is_global) {
    int& num_customers = is_global ? num_global_customers_ : num_local_customers_;
    auto& tables = is_global ? global_tables_ : local_tables_;
    --num_customers;
    assert(num_customers >= 0);
    assert(tables.size() > 0);
    if (table_probs_.size() < tables.size()) {
      table_probs_.resize(tables.size());
    }
    for (size_t i = 0; i < tables.size(); ++i) table_probs_[i] = tables[i];
    size_t table = random->SampleUnnormalizedPdfRef(table_probs_, tables.size() - 1);
    assert(table < tables.size());
    --tables[table];
    if (tables[table] == 0) {
      tables.erase(tables.begin() + table);
      return true;
    }
    return false;  
  }
  int num_global_customers() const { return num_global_customers_; }
  int num_local_customers() const { return num_local_customers_; }
  int num_global_tables() const { return global_tables_.size(); }
  int num_local_tables() const { return local_tables_.size(); }
  
 private:
  int num_global_customers_;
  int num_local_customers_;
  std::vector<int> global_tables_;
  std::vector<int> local_tables_;
  
  // buffer
  static std::vector<double> table_probs_;

  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(num_global_customers_)
        & MEMBER(num_local_customers_)
        & MEMBER(global_tables_)
        & MEMBER(local_tables_);
  }
};

} // hpy_lda

#endif /* _HPY_LDA_TABLE_RESTAURANT_HPP_ */
