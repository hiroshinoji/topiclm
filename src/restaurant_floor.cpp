#include <sstream>
#include <iostream>
#include "restaurant_floor.hpp"
#include "random_util.hpp"

using namespace std;

namespace topiclm {

AddRemoveResult RestaurantFloor::AddCustomer(int type,
                                             double global_parent_probability,
                                             double local_parent_probability,
                                             double lambda,
                                             double discount,
                                             double concentration) {
  auto& section = sections_[type];
  auto& tables = section.tables;
  ++sum_customers_;
  ++section.customers;

  double global_base_probability = global_parent_probability * (1 - lambda);
  double local_base_probability = local_parent_probability * lambda;

  if (section.customers == 1) {
    bool parent_is_global =
        random->NextBernoille(global_base_probability /
                              (global_base_probability + local_base_probability));
    Table t;
    t.parent_is_global = parent_is_global;
    t.customers = 1;

    tables.push_back(t);
    sum_tables_++;
    return parent_is_global ? GlobalTableChanged : LocalTableChanged;
  }
  vector<double> sitting_table_probs(tables.size() + 2, 0);
  for (size_t i = 0; i < tables.size(); ++i) {
    sitting_table_probs[i] = tables[i].customers - discount;
  }
  double common_newtable_prob = (concentration + discount * sum_tables_);
  sitting_table_probs[tables.size()] = common_newtable_prob * global_base_probability;
  sitting_table_probs[tables.size() + 1] = common_newtable_prob * local_base_probability;

  size_t sitting_table = random->SampleUnnormalizedPdf(sitting_table_probs);
  if (sitting_table >= tables.size()) {
    Table t;
    if (sitting_table == tables.size()) {
      t.parent_is_global = true;
    } else {
      t.parent_is_global = false;
    }
    t.customers = 1;
    tables.push_back(t);
    sum_tables_++;
    return t.parent_is_global ? GlobalTableChanged : LocalTableChanged;
  } else {
    tables[sitting_table].customers++;
    return TableUnchanged;
  }
}

AddRemoveResult RestaurantFloor::RemoveCustomer(int type,
                                                double /*discount*/) {
  auto& section = sections_[type];
  auto& tables = section.tables;
  --sum_customers_;
  --section.customers;
  vector<double> table_probs(tables.size());
  for (size_t i = 0; i < tables.size(); ++i) {
    table_probs[i] = tables[i].customers;
  }
  int removing_table = random->SampleUnnormalizedPdf(table_probs);
  --tables[removing_table].customers;
  if (tables[removing_table].customers == 0) {
    bool remove_is_global = tables[removing_table].parent_is_global;
    tables.erase(tables.begin() + removing_table);
    --sum_tables_;
    if (tables.empty()) {
      //sections_.set_deleted_key(-3);
      sections_.erase(type);
    }
    return remove_is_global ? GlobalTableChanged : LocalTableChanged;
  } else {
    return TableUnchanged;
  }
}

void RestaurantFloor::CheckConsistency() const {
  int sum_customers = 0;
  int sum_tables = 0;
  for (auto& section : sections_) {
    int customers_in_section = 0;
    for (auto& table : section.second.tables) {
      customers_in_section += table.customers;
    }
    if (customers_in_section != section.second.customers) {
      std::stringstream ss;
      ss << "consistency broken at section " << section.first << std::endl;
      ss << "sum_k(cvwk) [" << customers_in_section << "] != cvw ["
         << section.second.customers << "]" << std::endl;
      throw ss.str();
    }
    sum_customers += section.second.customers;
    sum_tables += section.second.tables.size();
  }
  if (sum_customers != sum_customers_) {
    std::stringstream ss;
    ss << "consistency broken in floor " << std::endl;
    ss << "sum_w(cvw) [" << sum_customers << "] != cv ["
       << sum_customers_ << "]" << std::endl;
    throw ss.str();
  }
  if (sum_tables != sum_tables_) {
    std::stringstream ss;
    ss << "consistency broken in floor " << std::endl;
    ss << "sum_w(tvw) [" << sum_tables << "] != tv ["
       << sum_tables_ << "]" << std::endl;
    throw ss.str();
  }
};

}; // namespace topiclm
