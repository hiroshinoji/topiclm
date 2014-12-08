#ifndef _HPY_LDA_INTERNAL_RESTAURANT_HPP_
#define _HPY_LDA_INTERNAL_RESTAURANT_HPP_

#include <vector>
#include <unordered_map>
#include <string>
#include <pficommon/text/json.h>
#include "random_util.hpp"
#include "serialization.hpp"
#include "section.hpp"
#include "add_remove_result.hpp"
#include "config.hpp"
#include "table_info.hpp"

namespace hpy_lda {

struct SampleTableInfo {
  topic_t label;
  int customers;
  std::vector<std::shared_ptr<Word> > observeds;
  bool exist;
  SampleTableInfo() : label(0), customers(0), exist(false) {}
  SampleTableInfo(topic_t label, int customers, std::vector<std::shared_ptr<Word> > observeds)
      : label(label), customers(customers), observeds(observeds), exist(true) {}
};

template <typename K>
class InternalRestaurant {
  friend class Restaurant;
 public:
  InternalRestaurant() {}
  std::pair<AddRemoveResult, topic_t> AddCustomer(
      K type,
      double local_parent_probability,
      const std::vector<double>& global_parent_pdf,
      double lambda,
      const std::vector<double>& label_priors,
      double discount,
      double concentration,
      int sum_tables);
  AddRemoveResult AddCustomerNewTable(K type, double lambda);
  std::pair<AddRemoveResult, topic_t> RemoveCustomer(K type, bool cache = false);
  void ChangeTableLabelAtRandom(K type, topic_t old_label, topic_t new_label);
  void ChangeTableLabel(K type, int customers, topic_t old_label, topic_t new_label);

  void ChangeTablesFloor(K old_type, K new_type, topic_t table_label, const std::vector<int>& table_customers);

  void AddCacheCustomerToExistingTables(K cache_id, double discount);
  void AddCacheCustomerToNewTable(K cache_id, topic_t topic);
  SampleTableInfo SampleTableAtRandom(K type);
  
  /*
    return size
   */
  size_t GetTableSeqFromSection(std::vector<TableInfo>& table_seq,
                                bool consider_zero = false);

  void CombineSectionToWord(K type, const std::shared_ptr<Word>& word_ptr, bool cache = false);
  void SeparateWordFromSection(K type, const std::shared_ptr<Word>& word_ptr, bool cache = false);

  void ResetCache() {
    //boost::container::flat_map<K, HistgramSection>().swap(cache_sections_);
  }

  int customers(K type) const {
    auto section_it = sections_.find(type);
    return section_it != sections_.end() ? (*section_it).second.customers : 0;
  }
  int tables(K type) const {
    auto section_it = sections_.find(type);
    return section_it != sections_.end() ? (*section_it).second.tables : 0;
  }
  int labeled_customers(K type, topic_t label) const {
    auto section_it = sections_.find(type);
    int customers = 0;
    if (section_it != sections_.end()) {
      auto& histogram = (*section_it).second.customer_histogram;
      auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), label);
      if (it != histogram.end() && (*it).first == label) {
        for (auto& datum : (*it).second) {
          customers += datum.first * datum.second;
        }
      }
    }
    return customers;
  }
  int labeled_tables(K type, topic_t label) const {
    auto section_it = sections_.find(type);
    int labeled_tables = 0;
    if (section_it != sections_.end()) {
      auto& histogram = (*section_it).second.customer_histogram;
      auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), label);
      if (it != histogram.end() && (*it).first == label) {
        for (auto& datum : (*it).second) {
          labeled_tables += datum.second;
        }
      }
    }
    return labeled_tables;
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
  void table_histogram(K type, bool /*cache*/, std::map<int, int>& histogram) const {
    //auto& target_sections = cache ? cache_sections_ : sections_;
    auto& target_sections = sections_;
    auto it = target_sections.find(type);
    if (it != target_sections.end()) {
      for (auto& label2histo : (*it).second.customer_histogram) {
        for (auto& datum : label2histo.second) {
          histogram[datum.first] += datum.second;
        }
      }
    }
  }
  std::vector<K> type_vector() const {
    std::vector<K> type_vector;
    for (auto& section : sections_) {
      type_vector.push_back(section.first);
    }
    return type_vector;
  }
  HistgramSection& section(K type) { return sections_[type]; }
  
  std::string to_string() const {
    std::stringstream ss;
    for (auto& kv : sections_) {
      K k = kv.first;
      const HistgramSection& section = kv.second;
      ss << "k=" << k << ", customers=" << section.customers << std::endl;
      ss << "k=" << k << ", tables=" << section.tables << std::endl;
      for (auto& histo_kv : section.customer_histogram) {
        topic_t label = histo_kv.first;
        auto& histogram = histo_kv.second;
        ss << "k=" << k << ", label=" << label << ", ";
        for (auto& c_t : histogram) {
          int c = c_t.first;
          int t = c_t.second;
          ss << "[";
          for (int i = 0; i < t; ++i) {
            ss << c << ", ";
          }
          ss << "]" << std::endl;
        }
      }
    }
    return ss.str();
  }
  
 private:
  void ResizeBuffer(size_t new_size) {
    table_probs_.resize(new_size);
    table_label_idxs_.resize(new_size);
    table_customer_idxs_.resize(new_size);
  }
  boost::container::flat_map<K, HistgramSection> sections_;
  //boost::container::flat_map<K, HistgramSection> cache_sections_;

  //buffer
  static std::vector<double> table_probs_;
  static std::vector<size_t> table_label_idxs_;
  static std::vector<size_t> table_customer_idxs_;

  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(sections_);
  }
};
template <typename K>
std::vector<double> InternalRestaurant<K>::table_probs_;
template <typename K>
std::vector<size_t> InternalRestaurant<K>::table_label_idxs_;
template <typename K>
std::vector<size_t> InternalRestaurant<K>::table_customer_idxs_;

template <typename K>
std::pair<AddRemoveResult, topic_t> InternalRestaurant<K>::AddCustomer(
    K type,
    double local_parent_probability,
    const std::vector<double>& global_parent_pdf,
    double lambda,
    const std::vector<double>& label_priors,
    double discount,
    double concentration,
    int sum_tables) {
  assert(global_parent_pdf.size() == label_priors.size());
  auto& section = sections_[type];
  auto& histogram = section.customer_histogram;
  ++section.customers;

  size_t labels_size = label_priors.size();
  if (section.customers == 1) {
    if (table_probs_.size() <= labels_size) {
      ResizeBuffer(labels_size + 1);
    }
    for (size_t i = 0; i < labels_size; ++i) {
      table_probs_[i] = (1 - lambda) * label_priors[i] * global_parent_pdf[i];
    }
    table_probs_[labels_size] = lambda * local_parent_probability;
    topic_t sample_label = random->SampleUnnormalizedPdfRef(table_probs_, labels_size);
    
    if (sample_label == (topic_t)labels_size) {
      sample_label = -1;
    }
    histogram.push_back({sample_label, {1, {1, 1}}});
    ++section.tables;
    return {sample_label == -1 ? LocalTableChanged : GlobalTableChanged, sample_label};
  }
  
  size_t buckets = section.bucket_size();
  
  if (table_probs_.size() <= (buckets + labels_size)) {
    ResizeBuffer(buckets + labels_size + 1);
  }
  for (size_t i = 0, k = 0; i < histogram.size(); ++i) {
    for (size_t j = 0; j < histogram[i].second.size(); ++j, ++k) {
      auto& bucket = histogram[i].second[j];
      table_probs_[k] = (bucket.first - discount) * bucket.second;
      table_label_idxs_[k] = i;
      table_customer_idxs_[k] = j;
    }
  }
  double common_newtable_prob = (concentration + discount * sum_tables);
  for (size_t i = 0; i < labels_size; ++i) {
    table_probs_[buckets + i] = common_newtable_prob * (1 - lambda) * label_priors[i] * global_parent_pdf[i];
  }
  table_probs_[buckets + labels_size] = common_newtable_prob * lambda * local_parent_probability;
  size_t sample = random->SampleUnnormalizedPdfRef(table_probs_, buckets + labels_size);
  
  if (sample >= buckets) {
    topic_t sample_label = sample - buckets;
    if (sample_label == (topic_t)labels_size) {
      sample_label = -1;
    }
    section.AddNewTable(sample_label, 1);
    return {sample_label == -1 ? LocalTableChanged : GlobalTableChanged, sample_label};
  } else {
    section.ChangeNumCustomer(table_label_idxs_[sample], table_customer_idxs_[sample], 1);
    return {TableUnchanged, 0};
  }
}

template <typename K>
AddRemoveResult InternalRestaurant<K>::AddCustomerNewTable(K type, double lambda) {
  auto& section = sections_[type];
  ++section.customers;
  
  bool parent_is_global = random->NextBernoille(1 - lambda);
  topic_t label = parent_is_global ? 0 : -1;
  section.AddNewTable(label, 1);
  
  return parent_is_global ? GlobalTableChanged : LocalTableChanged;
}

template <typename K>
std::pair<AddRemoveResult, topic_t> InternalRestaurant<K>::RemoveCustomer(K type, bool /*cache*/) {
  //auto& target_sections = cache ? cache_sections_ : sections_;
  auto& target_sections = sections_;
  auto& section = target_sections[type];
  auto& histogram = section.customer_histogram;
  
  --section.customers;
  assert(section.customers >= 0);

  if (section.customers == 0) {
    topic_t label = histogram[0].first;
    EraseAndShrink(target_sections, type);
    return {label == -1 ? LocalTableChanged : GlobalTableChanged, label};
  }
  size_t buckets = section.bucket_size();  
  if (table_probs_.size() < buckets) {
    ResizeBuffer(buckets);
  }

  for (size_t i = 0, k = 0; i < histogram.size(); ++i) {
    for (size_t j = 0; j < histogram[i].second.size(); ++j, ++k) {
      auto& datum = histogram[i].second[j];
      table_probs_[k] = datum.first * datum.second;
      table_label_idxs_[k] = i;
      table_customer_idxs_[k] = j;
    }
  }
  int sample = random->SampleUnnormalizedPdfRef(table_probs_, buckets - 1);

  auto& label_histo = histogram[table_label_idxs_[sample]];
  auto& bucket = label_histo.second[table_customer_idxs_[sample]];

  if (bucket.first == 1) {
    topic_t remove_label = label_histo.first;
    section.RemoveTable(table_label_idxs_[sample], table_customer_idxs_[sample]);
    return {remove_label == -1 ? LocalTableChanged : GlobalTableChanged, remove_label};
  } else {
    section.ChangeNumCustomer(table_label_idxs_[sample], table_customer_idxs_[sample], -1);
    return {TableUnchanged, 0};
  }
}

template <typename K>
void InternalRestaurant<K>::ChangeTableLabelAtRandom(K type, topic_t old_label, topic_t new_label) {
  auto& section = sections_[type];
  auto& histogram = section.customer_histogram;

  auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), old_label);
  assert((*it).first == old_label);
  auto& label_histo = (*it).second;

  for (size_t i = 0; i < label_histo.size(); ++i) {
    table_probs_[i] = label_histo[i].second;
  }
  int sample = random->SampleUnnormalizedPdfRef(table_probs_, label_histo.size() - 1);
  int new_customers = label_histo[sample].first;

  section.RemoveTable(it - histogram.begin(), sample);
  section.AddNewTable(new_label, new_customers);
}
template <typename K>
void InternalRestaurant<K>::ChangeTableLabel(
    K type, int customers, topic_t old_label, topic_t new_label) {
  auto& section = sections_[type];
  auto& histogram = section.customer_histogram;
  
  auto it = pairVectorLowerBound(histogram.begin(), histogram.end(), old_label);
  assert(it != histogram.end() && (*it).first == old_label);
  auto& label_histo = (*it).second;

  auto histo_it = pairVectorLowerBound(label_histo.begin(), label_histo.end(), customers);
  assert(histo_it != label_histo.end() && (*histo_it).first == customers);
  section.RemoveTable(it - histogram.begin(), histo_it - label_histo.begin());
  section.AddNewTable(new_label, customers);
}
template <typename K>
void InternalRestaurant<K>::ChangeTablesFloor(K old_type, K new_type, topic_t table_label, const std::vector<int>& table_customers) {
  auto& old_section = sections_[old_type];
  auto& old_histogram = old_section.customer_histogram;
  auto it = pairVectorLowerBound(old_histogram.begin(), old_histogram.end(), table_label);
  assert((*it).first == table_label);
  auto& label_histo = (*it).second;

  for (int c : table_customers) {
    old_section.customers -= c;
    assert(old_section.customers >= 0);
    auto histo_it = pairVectorLowerBound(label_histo.begin(), label_histo.end(), c);
    assert(histo_it != label_histo.end() && (*histo_it).first == c);
    old_section.RemoveTable(it - old_histogram.begin(), histo_it - label_histo.begin());
  }
  if (old_section.customers == 0) {
    assert(old_section.tables == 0);
    EraseAndShrink(sections_, old_type);
  }
  auto& new_section = sections_[new_type];
  for (int c : table_customers) {
    new_section.customers += c;
    new_section.AddNewTable(table_label, c);
  }
}

template <typename K>
void InternalRestaurant<K>::AddCacheCustomerToExistingTables(K /*cache_id*/, double /*discount*/) {
  // auto& section = cache_sections_[cache_id];
  // auto& histogram = section.customer_histogram;
  // ++section.customers;
  // assert(section.customers >= 2);

  // size_t buckets = section.bucket_size();

  // if (table_probs_.size() < buckets) {
  //   ResizeBuffer(buckets);
  // }
  // for (size_t i = 0, k = 0; i < histogram.size(); ++i) {
  //   for (size_t j = 0; j < histogram[i].second.size(); ++j, ++k) {
  //     auto& bucket = histogram[i].second[j];
  //     table_probs_[k] = (bucket.first - discount) * bucket.second;
  //     table_label_idxs_[k] = i;
  //     table_customer_idxs_[k] = j;
  //   }
  // }
  // size_t sample = random->SampleUnnormalizedPdf(table_probs_, buckets - 1);

  // section.ChangeNumCustomer(table_label_idxs_[sample], table_customer_idxs_[sample], 1);
  // assert(section.customers >= section.tables);
}
template <typename K>
void InternalRestaurant<K>::AddCacheCustomerToNewTable(K /*cache_id*/, topic_t /*topic*/) {
  // auto& section = cache_sections_[cache_id];
  // ++section.customers;

  // section.AddNewTable(topic, 1);
  // assert(section.customers >= section.tables);
}
template <typename K>
SampleTableInfo InternalRestaurant<K>::SampleTableAtRandom(K type) {
  auto it = sections_.find(type);
  if (it == sections_.end()) {
    return SampleTableInfo();
  }
  auto& section = (*it).second;
  auto& histogram = section.customer_histogram;
  size_t buckets = section.bucket_size();  
  if (table_probs_.size() < buckets) {
    ResizeBuffer(buckets);
  }

  for (size_t i = 0, k = 0; i < histogram.size(); ++i) {
    for (size_t j = 0; j < histogram[i].second.size(); ++j, ++k) {
      auto& datum = histogram[i].second[j];
      table_probs_[k] = datum.second;
      table_label_idxs_[k] = i;
      table_customer_idxs_[k] = j;
    }
  }
  int sample = random->SampleUnnormalizedPdfRef(table_probs_, buckets - 1);
  topic_t label = histogram[table_label_idxs_[sample]].first;
  int c = histogram[table_label_idxs_[sample]].second[table_customer_idxs_[sample]].first;
  bool observed = random->NextBernoille(double(section.observeds.size()) / double(section.customers));
  std::vector<std::shared_ptr<Word> > observeds;
  if (observed) {
    if (table_label_idxs_.size() < section.observeds.size()) {
      ResizeBuffer(section.observeds.size());
    }
    for (size_t i = 0; i < section.observeds.size(); ++i) {
      table_label_idxs_[i] = i;
    }
    random_shuffle(table_label_idxs_.begin(), table_label_idxs_.begin() + section.observeds.size(), *random);
    observeds.resize(c);
    for (size_t i = 0; i < observeds.size(); ++i) {
      observeds[i] = section.observeds[table_label_idxs_[i]];
    }
  }
  return {label, c, observeds};
}
template <typename K>
size_t InternalRestaurant<K>::GetTableSeqFromSection(std::vector<TableInfo>& table_seq,
                                                     bool consider_zero) {
  size_t i = 0;
  for (auto& type2section : sections_) {
    auto type = type2section.first;
    if (type == 0 && !consider_zero) continue;
    auto& section = type2section.second;
    auto& observeds = section.observeds;

    int c_obs = observeds.size();
    int c_sum = section.customers;
    size_t section_start = i;
    for (auto& label2histo : section.customer_histogram) {
      auto label = label2histo.first;
      auto& histo = label2histo.second;
      for (auto& bucket : histo) { // num customer -> num tables
        for (int j = 0; j < bucket.second; ++j) {
          int obs_size = 0;
          if (c_obs) { // in section which has some customers who is connected to a word.
            int c = bucket.first;
            for (int l = 0; l < c; ++l) {
              if (random->NextBernoille(double(c_obs) / double(c_sum))) {
                assert(--c_obs >= 0);
                assert(--c_sum >= 0);
                ++obs_size;
              } else {
                assert(--c_sum >= 0);
              }
            }
          } else {
            c_sum -= bucket.first;
          }
          if (i >= table_seq.size()) table_seq.resize(i + 1);
          table_seq[i++] = {type, bucket.first, label, obs_size, {}};
        }
      }
    }
    assert(c_obs == 0);
    assert(c_sum == 0);
    if (observeds.size() > 0) {
      if (table_label_idxs_.size() < section.observeds.size()) {
        ResizeBuffer(section.observeds.size());
      }
      for (size_t j = 0; j < section.observeds.size(); ++j) {
        table_label_idxs_[j] = j;
      }
      random_shuffle(table_label_idxs_.begin(), table_label_idxs_.begin() + section.observeds.size(), *random);
      for (size_t j = section_start, k = 0; j < i; ++j) {
        assert(table_seq[j].floor == type);
        for (int l = 0; l < table_seq[j].obs_size; ++l, ++k) {
          table_seq[j].observeds.push_back(observeds[table_label_idxs_[k]]);
        }
      }
    }
  }
  return i;
}
template <typename K>
void InternalRestaurant<K>::CombineSectionToWord(K type, const std::shared_ptr<Word>& word_ptr, bool /*cache*/) {
  //auto& target_sections = cache ? cache_sections_ : sections_;
  auto& target_sections = sections_;
  auto& section = target_sections[type];
  
  auto it = std::lower_bound(section.observeds.begin(), section.observeds.end(), word_ptr);
  assert(it == section.observeds.end() || *it != word_ptr);
  section.observeds.insert(it, word_ptr);
}
template <typename K>
void InternalRestaurant<K>::SeparateWordFromSection(K type, const std::shared_ptr<Word>& word_ptr, bool /*cache*/) {
  //auto& target_sections = cache ? cache_sections_ : sections_;
  auto& target_sections = sections_;
  auto& section = target_sections[type];

  auto it = std::lower_bound(section.observeds.begin(), section.observeds.end(), word_ptr);
  if (!(it != section.observeds.end() && *it == word_ptr)) {
    std::cerr << "\nword_ptr: " << word_ptr.get() << std::endl;
    std::cerr << "find: " << (*it).get() << std::endl;
    for (auto& word : section.observeds) {
      std::cerr << word.get() << " ";
    }
    std::cerr << std::endl;
    throw "hoge";
  }
  assert(it != section.observeds.end() && *it == word_ptr);
  EraseAndShrink(section.observeds, it);
}

} // hpy_lda


#endif /* _HPY_LDA_INTERNAL_RESTAURANT_HPP_ */
