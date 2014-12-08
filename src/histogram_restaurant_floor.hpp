#ifndef _HPY_LDA_HISTGRAM_RESTAURANT_FLOOR_H_
#define _HPY_LDA_HISTGRAM_RESTAURANT_FLOOR_H_

#include <vector>
#include <unordered_map>
#include <string>
#include <pficommon/text/json.h>
#include "serialization.hpp"
#include "section.hpp"
#include "add_remove_result.hpp"
#include "config.hpp"

namespace hpy_lda {

template <typename SectionKey>
class InternalRestaurant {
  friend class Restaurant;
 public:
  HistogramRestaurantSection() {}
  std::pair<AddRemoveResult, topic_t> AddCustomer(
      topic_t topic,
      double local_parent_probability,
      const std::vector<double>& global_parent_pdf,
      double lambda,
      const std::vector<double>& label_priors,
      double discount,
      double concentration,
      int sum_tables);
  AddRemoveResult AddCustomerNewTable(topic_t topic, double lambda);
  std::pair<AddRemoveResult, topic_t> RemoveCustomer(topic_t topic);
  void ChangeOneTableLabel(topic_t topic, topic_t old_label, topic_t new_label);

  void ResetCache() {
    boost::container::flat_map<topic_t, HistgramSection>().swap(cache_sections_);
  }
  
  void CheckConsistency() const;

  int customers(topic_t topic) const {
    auto section_it = sections_.find(topic);
    if (section_it != sections_.end()) {
      return (*section_it).second.customers;
    } else {
      return 0;
    }
  }
  int labeled_customers(topic_t topic, topic_t label) const {
    auto section_it = sections_.find(topic);
    int customers = 0;
    if (section_it != sections_.end()) {
      auto& customer_histogram = (*section_it).second.customer_histogram;
      auto it = pairVectorLowerBound(customer_histogram.begin(), customer_histogram.end(), label);
      if (it != customer_histogram.end() && (*it).first == label) {
        for (auto& datum : (*it).second) {
          customers += datum.first * datum.second;
        }
        return customers;
      }
    }
    return 0;
  }
  
  int tables(topic_t topic) const {
    auto section_it = sections_.find(topic);
    if (section_it != sections_.end()) {
      return (*section_it).second.tables;
    } else {
      return 0;
    }
  }
  int labeled_tables(topic_t topic, topic_t label) const {
    auto section_it = sections_.find(topic);
    int labeled_tables = 0;
    if (section_it != sections_.end()) {
      auto& customer_histogram = (*section_it).second.customer_histogram;
      auto it = pairVectorLowerBound(customer_histogram.begin(), customer_histogram.end(), label);
      if (it != customer_histogram.end() && (*it).first == label) {
        for (auto& datum : (*it).second) {
          labeled_tables += datum.second;
        }
        return labeled_tables;
      }
    }
    return 0;
  }
  
  std::vector<int> table_vector() const {
    std::vector<int> table_vector;
    for (auto& section : sections_) {
      for (auto& label2histo : section.second.customer_histogram) {
        for (auto& datum : label2histo.second) {
          int sitting_customers = datum.first;
          int tables = datum.second;
          for (int i = 0; i < tables; ++i) {
            table_vector.push_back(sitting_customers);
          }
        }
      }
    }
    return table_vector;
  }
  std::vector<int> floor_table_vector(topic_t floor_id) const {
    std::vector<int> table_vector;
    for (auto& section : sections_) {
      auto& histogram = section.second.customer_histogram;
      auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), floor_id);
      if (it != histogram.end() && (*it).first == floor_id) {
        for (auto& bucket : (*it).second) {
          int sitting_customers = bucket.first;
          int tables = bucket.second;
          for (int i = 0; i < tables; ++i) {
            table_vector.push_back(sitting_customers);
          }
        }
      }
    }
    return table_vector;
  }
  std::vector<int> floor_vector() const {
    std::vector<int> type_vector;
    for (auto& section : sections_) {
      type_vector.push_back(section.first);
    }
    return type_vector;
  }
  
 private:
  void ResizeBuffer(size_t new_size) {
    table_probs_.resize(new_size);
    table_label_idxs_.resize(new_size);
    table_customer_idxs_.resize(new_size);
  }
  boost::container::flat_map<topic_t, HistgramSection> sections_;
  boost::container::flat_map<topic_t, HistgramSection> cache_sections_;

  // buffer
  static std::vector<double> table_probs_;
  static std::vector<size_t> table_label_idxs_;
  static std::vector<size_t> table_customer_idxs_;

  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(sections_);
  }
};

} // namespace hpy_lda

#endif /* _HPY_LDA_HISTGRAM_RESTAURANT_FLOOR_H_ */
