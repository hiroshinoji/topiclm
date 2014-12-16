#include <sstream>
#include <iostream>
#include "histogram_restaurant_floor.hpp"
#include "random_util.hpp"
#include "util.hpp"

using namespace std;

namespace topiclm {

vector<double> HistogramRestaurantFloor::table_probs_ = std::vector<double>();
vector<size_t> HistogramRestaurantFloor::table_label_idxs_ = std::vector<size_t>();
vector<size_t> HistogramRestaurantFloor::table_customer_idxs_ = std::vector<size_t>();

pair<AddRemoveResult, topic_t> HistogramRestaurantFloor::AddCustomer(
    topic_t topic, 
    double local_parent_probability,
    const std::vector<double>& global_parent_pdf,
    double lambda,
    const std::vector<double>& label_priors,
    double discount,
    double concentration,
    int sum_tables) {
  assert(global_parent_pdf.size() == label_priors.size());
  auto& section = sections_[topic];
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
AddRemoveResult HistogramRestaurantFloor::AddCustomerNewTable(topic_t topic, double lambda) {
  auto& section = sections_[topic];
  ++section.customers;

  bool parent_is_global = random->NextBernoille(1 - lambda);
  topic_t label = parent_is_global ? 0 : -1;
  section.AddNewTable(label, 1);
  
  return parent_is_global ? GlobalTableChanged : LocalTableChanged;
}

pair<AddRemoveResult, topic_t> HistogramRestaurantFloor::RemoveCustomer(topic_t topic) {
  auto& section = sections_[topic];
  auto& histogram = section.customer_histogram;
  
  --section.customers;
  assert(section.customers >= 0);

  if (section.customers == 0) {
    topic_t label = histogram[0].first;
    EraseAndShrink(sections_, topic);
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

void HistogramRestaurantFloor::ChangeOneTableLabel(topic_t topic, topic_t old_label, topic_t new_label) {
  auto& section = sections_[topic];
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

void HistogramRestaurantFloor::CheckConsistency() const {
}


};
