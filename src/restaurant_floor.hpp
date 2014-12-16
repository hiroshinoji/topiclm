#ifndef _TOPICLM_RESTAURANT_FLOOR_H_
#define _TOPICLM_RESTAURANT_FLOOR_H_

#include <vector>
#include <unordered_map>
#include <string>
#include "serialization.hpp"
#include "section.hpp"
#include "add_remove_result.hpp"
#include "config.hpp"

namespace topiclm {

class RestaurantFloor {
 public:
  RestaurantFloor() : sum_customers_(0), sum_tables_(0) {}
  AddRemoveResult AddCustomer(int type,
                              double global_parent_probability,
                              double local_parent_probability,
                              double lambda,
                              double discount,
                              double concentration);
  
  AddRemoveResult RemoveCustomer(int type,
                                 double discount);
  
  void CheckConsistency() const;

  Section& section(int type) {
    return sections_[type];
  }
  int customers(int type) const {
    auto section_it = sections_.find(type);
    if (section_it != sections_.end()) {
      return (*section_it).second.customers;
    } else {
      return 0;
    }
  }
  int global_labeled_customers(int type) const {
    auto section_it = sections_.find(type);
    int customers = 0;
    if (section_it != sections_.end()) {
      auto& tables = (*section_it).second.tables;
      for (auto& table : tables) {
        if (table.parent_is_global) {
          customers += table.customers;
        }
      }
    }
    return customers;
  }
  
  int tables(int type) const {
    auto section_it = sections_.find(type);
    if (section_it != sections_.end()) {
      return (*section_it).second.tables.size();
    } else {
      return 0;
    }
  }
  int global_labeled_tables(int type) const {
    auto section_it = sections_.find(type);
    int global_tables = 0;
    if (section_it != sections_.end()) {
      auto& tables = (*section_it).second.tables;
      for (auto& table : tables) {
        if (table.parent_is_global) ++global_tables;
      }
    }
    return global_tables;
  }
  
  int sum_customers() const { return sum_customers_; }
  int sum_tables() const { return sum_tables_; }
  std::vector<int> table_vector() const {
    std::vector<int> table_vector;
    for (auto& section : sections_) {
      for (auto& table : section.second.tables) {
        table_vector.push_back(table.customers);
      }
    }
    return table_vector;
  }
  std::vector<int> type_vector() const {
    std::vector<int> type_vector;
    for (auto& section : sections_) {
      type_vector.push_back(section.first);
    }
    return type_vector;
  }
 private:
  //google::sparse_hash_map<int, Section> sections_;
  std::unordered_map<int, Section> sections_;
  int sum_customers_;
  int sum_tables_;

  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(sections_)
        & MEMBER(sum_customers_)
        & MEMBER(sum_tables_);
  }
};

} // namespace topiclm

#endif /* _TOPICLM_RESTAURANT_FLOOR_H_ */
