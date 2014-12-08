#include "util.hpp"
#include "random_util.hpp"
#include "floor_sampler.hpp"

using namespace std;

namespace hpy_lda {

/*===================================
 *
 * ArrangementProbNumeratorCache
 *
 *==================================*/
ArrangementProbNumeratorCache::ArrangementProbNumeratorCache() {}
void ArrangementProbNumeratorCache::Init(int num_tables_already, double discount, double concentration) {
  num_tables_already_ = num_tables_already;
  discount_ = discount;
  concentration_ = concentration;
  SetInitialValues();
}
void ResizeArrangeNumerCache(double discount,
                             double concentration,
                             size_t new_size,
                             std::vector<ArrangementProbNumeratorCache>& caches) {
  size_t old_size = caches.size();
  caches.resize(new_size);
  for (size_t i = old_size; i < new_size; ++i) {
    caches[i].Init(i, discount, concentration);
  }
}

/*===================================
 *
 * RisingFactorialCache
 *
 *==================================*/
RisingFactorialCache::RisingFactorialCache() {}
void RisingFactorialCache::Init(int num_already, double hyper) {
  num_already_ = num_already;
  hyper_ = hyper;
  SetInitialValues();
}
void ResizeFactorialCache(double concentration,
                          size_t new_size,
                          std::vector<RisingFactorialCache>& caches) {
  size_t old_size = caches.size();
  caches.resize(new_size);
  for (size_t i = old_size; i < new_size; ++i) {
    caches[i].Init(i, concentration);
  }
}

/*===================================
 *
 * SeatingArrangementProbCalculator
 *
 *==================================*/
SeatingArrangementProbCalculator::SeatingArrangementProbCalculator(
    double discount, double concentration)
    : discount_(discount), concentration_(concentration) {}

double SeatingArrangementProbCalculator::log_p(
    size_t c_block, size_t t_block, size_t c_restaurant, size_t t_restaurant) {
  if (num_caches_.size() <= t_restaurant) {
    ResizeArrangeNumerCache(discount_, concentration_, t_restaurant + 1, num_caches_);
  }
  if (denom_caches_.size() <= c_restaurant) {
    ResizeFactorialCache(concentration_, c_restaurant + 1, denom_caches_);
  }
  return num_caches_[t_restaurant].get(t_block) - denom_caches_[c_restaurant].get(c_block);
}

/*===================================
 *
 * TopicPriorCalculator
 *
 *==================================*/
TopicPriorCalculator::TopicPriorCalculator(const std::vector<double>& topic2alpha)
    : topic2alpha_(topic2alpha),
      alpha_sum_(accumulate(topic2alpha.begin(), topic2alpha.end(), 0.0)) {
  topic2num_caches_.resize(topic2alpha_.size());
}
double TopicPriorCalculator::log_p(
    size_t k,
    size_t old_k,
    const unordered_map<int, int>& doc2move_customers,
    const vector<pair<int, vector<int> > >& doc2topic_count) {
  double accum = 0;
  for (auto& doc_customer : doc2move_customers) {
    int j = doc_customer.first;
    int n_js = doc_customer.second;
    int N_j = doc2topic_count[j].first - n_js;
    int n_jk = doc2topic_count[j].second[k];
    if (k == old_k) n_jk -= n_js;

    if (topic2num_caches_[k].size() <= (size_t)n_jk) {
      ResizeFactorialCache(topic2alpha_[k], n_jk + 1, topic2num_caches_[k]);
    }
    if (denom_caches_.size() <= (size_t)N_j) {
      ResizeFactorialCache(alpha_sum_, N_j + 1, denom_caches_);
    }
    accum += topic2num_caches_[k][n_jk].get(n_js) - denom_caches_[N_j].get(n_js);
  }
  return accum;
}

/*===================================
 *
 * FloorSampler
 *
 *==================================*/
FloorSampler::FloorSampler(size_t num_topics, bool include_zero)
  : pdf_(num_topics + 1), include_zero_(include_zero) {}

void FloorSampler::ResetArrangementCache(const vector<double>& depth2discounts,
                                         const vector<double>& depth2concentrations) {
  vector<unique_ptr<SeatingArrangementProbCalculator> > arrangement_part;
  for (size_t i = 0; i < depth2discounts.size(); ++i) {
    arrangement_part.emplace_back(
        new SeatingArrangementProbCalculator(depth2discounts[i],
                                             depth2concentrations[i]));
  }
  arrangement_part.swap(arrangement_part_);
}
void FloorSampler::ResetTopicPriorCache(const std::vector<double>& topic2alpha) {
  topic_part_.reset(new TopicPriorCalculator(topic2alpha));
}

void FloorSampler::TakeInSectionLL(
    const boost::container::flat_map<topic_t, std::pair<int, int> >& floor2c_t,
    int add_customers,
    int add_tables,
    int current_k,
    int depth) {
  int s = pdf_.size();
  auto it = floor2c_t.begin();
  int j = 0;
  if (!include_zero_) {
    j = 1;
    if (it != floor2c_t.end() && (*it).first == 0) ++it;
  }
  for (; j < s; ++j) {
    if (it == floor2c_t.end() || (*it).first > j) {
      assert(j != current_k); // current restaurant should never be empty
      pdf_[j] += arrangement_part_[depth]->log_p(add_customers, add_tables, 0, 0);
    } else {
      assert((*it).first == j);
      int c = (*it).second.first;
      int t = (*it).second.second;
      if (j == current_k) {
        c -= add_customers;
        t -= add_tables;
      }
      pdf_[j] += arrangement_part_[depth]->log_p(add_customers, add_tables, c, t);
      ++it;
    }
  }
}
void FloorSampler::TakeInPrior(int current_k,
                               const std::unordered_map<int, int>& doc2move_customers,
                               const std::vector<std::pair<int, std::vector<int> > >& doc2topic_count) {
  int s = pdf_.size();
  for (int k = 1; k < s; ++k) {
    pdf_[k] += topic_part_->log_p(k, current_k, doc2move_customers, doc2topic_count);
  }
}

void FloorSampler::TakeInLambda(const vector<int>& topic2global_tables,
                                const vector<int>& topic2local_tables,
                                int topic,
                                int add_tables,
                                double a,
                                double b) {
  int s = pdf_.size();
  for (int j = 1; j < s; ++j) {
    double t_l = topic2local_tables[j] + a;      
    double t_g = topic2global_tables[j] + b;
    if (j == topic) {
      t_l -= add_tables;
      assert(t_l >= 0);
    }
    pdf_[j] += lgamma(t_l + t_g) - lgamma(t_l + t_g + add_tables)
        + lgamma(t_l + add_tables) - lgamma(t_l);
  }
}

void FloorSampler::TakeInLambda(double global_lambda, double local_lambda) {
  pdf_[0] += std::log(global_lambda);
  for (size_t j = 1; j < pdf_.size(); ++j) {
    pdf_[j] += log(local_lambda);
  }
}

void FloorSampler::TakeInParentPredictive(const std::vector<double>& parent_predictives) {
  for (size_t i = 0; i < pdf_.size(); ++i) {
    pdf_[i] += log(parent_predictives[i]);
  }
}
int FloorSampler::Sample(int current) {
  for (size_t i = 0; i < pdf_.size(); ++i) {
    if (pdf_[i] == 0) {
      pdf_[i] = -INFINITY;
    }
  }
  if (!include_zero_) pdf_[0] = -INFINITY;
  normalize_logpdf(pdf_.begin(), pdf_.end());
  return random->SampleUnnormalizedPdfRef(pdf_);
}

} // hpy_lda
