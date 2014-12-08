#ifndef _HPY_LDA_SECTION_H_
#define _HPY_LDA_SECTION_H_

#include <vector>
#include <sstream>
#include "config.hpp"
#include "serialization.hpp"
#include "word.hpp"

namespace hpy_lda {

struct BinaryHistoKey {
  bool parent_is_global;
  int sitting_customers;
  BinaryHistoKey() : parent_is_global(false), sitting_customers(0) {}
  BinaryHistoKey(bool parent_is_global, int sitting_customers)
      : parent_is_global(parent_is_global), sitting_customers(sitting_customers) {}
  
  bool operator<(const BinaryHistoKey& lhs) const {
    if (sitting_customers == lhs.sitting_customers) {
      return parent_is_global < lhs.parent_is_global;
    }
    return sitting_customers < lhs.sitting_customers;
  }
  bool operator>(const BinaryHistoKey& lhs) const { return !operator<(lhs); }

  bool operator==(const BinaryHistoKey& lhs) const { 
    return parent_is_global == lhs.parent_is_global
        && sitting_customers == lhs.sitting_customers;
  }
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(parent_is_global)
        & MEMBER(sitting_customers);
  }
};


struct HistgramSection {
  std::vector<std::pair<topic_t, std::vector<std::pair<int, int> > > > customer_histogram; // -1 -> local
  int customers;
  int tables;
  std::vector<std::shared_ptr<Word> > observeds;
  HistgramSection() : customers(0), tables(0) {}

  void AddNewTable(topic_t label, int num_customer) {
    auto label_it = pairVectorLowerBound(customer_histogram.begin(), customer_histogram.end(), label);
    if (label_it != customer_histogram.end() && (*label_it).first == label) {
      auto& label_histo = (*label_it).second;
      auto customer_it = pairVectorLowerBound(label_histo.begin(), label_histo.end(), num_customer);
      if (customer_it != label_histo.end() && (*customer_it).first == num_customer) {
        ++(*customer_it).second;
      } else {
        label_histo.insert(customer_it, {num_customer, 1});
      }
    } else {
      customer_histogram.insert(label_it, {label, {1, {num_customer, 1}}});
    }
    ++tables;
  }
  void RemoveTable(size_t label_idx, size_t customer_idx) {
    auto& label_histo = customer_histogram[label_idx];
    auto& bucket = label_histo.second[customer_idx];
    assert(bucket.second >= 1);
    if (--bucket.second == 0) {
      EraseAndShrink(label_histo.second, label_histo.second.begin() + customer_idx);
      if (label_histo.second.empty()) {
        EraseAndShrink(customer_histogram, customer_histogram.begin() + label_idx);
      }
    }
    --tables;
  }
  void ChangeNumCustomer(size_t label_idx, size_t customer_idx, int customer_diff) {
    auto& label_histo = customer_histogram[label_idx];
    auto& bucket = label_histo.second[customer_idx];
    int new_customer = bucket.first + customer_diff;
    assert(new_customer > 0);
    if (--bucket.second == 0) {
      EraseAndShrink(label_histo.second, label_histo.second.begin() + customer_idx);
    }
    auto it = pairVectorLowerBound(label_histo.second.begin(), label_histo.second.end(), new_customer);
    if (it != label_histo.second.end() && (*it).first == new_customer) {
      ++(*it).second;
    } else {
      label_histo.second.insert(it, {new_customer, 1});
    }
  }
  size_t bucket_size() const {
    size_t s = 0;
    for (auto& label_histo : customer_histogram) {
      s += label_histo.second.size();
    }
    return s;
  }

  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(customer_histogram)
        & MEMBER(customers)
        & MEMBER(tables);
  }
};

};

#endif /* _HPY_LDA_SECTION_H_ */
