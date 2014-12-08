#ifndef _HPY_LDA_FLOOR_SAMPLER_HPP_
#define _HPY_LDA_FLOOR_SAMPLER_HPP_

#include <vector>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include "word.hpp"
#include "util.hpp"
#include "random_util.hpp"

namespace hpy_lda {

class CachedVector {
 public:
  virtual double redidual(int i) = 0;
  double get(size_t num_add) {
    if (values_.size() <= num_add) {
      size_t old_size = values_.size();
      values_.resize(num_add + 1);
      for (size_t i = old_size; i < values_.size(); ++i) {
        values_[i] = values_[i - 1] + std::log(redidual(i));
      }
    }
    return values_[num_add];
  }
  void SetInitialValues() {
    values_.clear();
    values_.resize(2);
    values_[0] = 0;
    values_[1] = std::log(redidual(1));
  }
 protected:
  std::vector<double> values_;
};

// calculate numerator of p(s|k) i.e. probability of a block of seating arrangement under topic k.
// the body of this class is CachedVector::get
// NOTE: Init must be called before any calls of get method
class ArrangementProbNumeratorCache : public CachedVector {
 public:
  ArrangementProbNumeratorCache();
  void Init(int num_tables_already, double discount, double concentration);
  
  double redidual(int i) {
    return concentration_ + discount_ * (num_tables_already_ + i - 1);
  }
  
 private:
  int num_tables_already_;
  double discount_;
  double concentration_;
  
};

// calcuate rising factorial (a+n)^(m) = (a+n)*(a+n+1)*...*(a+n+m-1)
// where a=hyper_, n=num_already_, m=num_add for each.
// used as a denominator for p(s|k) and a numerator/denominator for p(z_s=k|z^-s) i.e. topic prior
// NOTE: Init must be called before any calls of get method
class RisingFactorialCache : public CachedVector {
 public:
  RisingFactorialCache();
  void Init(int num_already, double hyper);
  
  double redidual(int i) {
    return hyper_ + num_already_ + (i - 1);
  }
 private:
  int num_already_;
  double hyper_;
  std::vector<double> values_;
  
};

class SeatingArrangementProbCalculator {
 public:
  SeatingArrangementProbCalculator(double discount, double concentration);
  double log_p(size_t c_block, size_t t_block, size_t c_restaurant, size_t t_restaurant);
  
 private:
  double discount_;
  double concentration_;
  std::vector<ArrangementProbNumeratorCache> num_caches_;
  std::vector<RisingFactorialCache> denom_caches_;
  
};

class TopicPriorCalculator {
 public:
  TopicPriorCalculator(const std::vector<double>& topic2alpha);
  double log_p(size_t k,
               size_t old_k,
               const std::unordered_map<int, int>& doc2move_customers,
               const std::vector<std::pair<int, std::vector<int> > >& doc2topic_count);
  
 private:
  std::vector<double> topic2alpha_;
  double alpha_sum_;
  
  // toic2num_caches_[k][n] indicates the rising factorial
  // of base=(a+n) where a is concentration for topic k.
  std::vector<std::vector<RisingFactorialCache> > topic2num_caches_;
  // denom_caches_[n] : denominator is not sensitive to the topic.
  std::vector<RisingFactorialCache> denom_caches_;
  
};

class FloorSampler {
 public:
  FloorSampler(size_t num_topics, bool include_zero);
  
  // should be called when the hyper parameters is changed.
  void ResetArrangementCache(const std::vector<double>& depth2discounts,
                             const std::vector<double>& depth2concentrations);
  void ResetTopicPriorCache(const std::vector<double>& topic2alpha);

  void ClearPdf() {
    fill(pdf_.begin(), pdf_.end(), 0.0);
  }
  
  void TakeInSectionLL(
      const boost::container::flat_map<topic_t, std::pair<int, int> >& floor2c_t,
      int add_customers,
      int add_tables,
      int current_k,
      int depth);
  
  void TakeInPrior(int current_floor,
                   const std::unordered_map<int, int>& doc2move_customers,
                   const std::vector<std::pair<int, std::vector<int> > >& doc2topic_count);
  
  //@duplicate: used only in DHPYTM with wood mode's lambda.
  void TakeInLambda(const std::vector<int>& topic2global_tables,
                    const std::vector<int>& topic2local_tables,
                    int topic,
                    int add_tables,
                    double a,
                    double b);
  // used in cHPYTM
  void TakeInLambda(double global_lambda, double local_lambda);
  void TakeInParentPredictive(const std::vector<double>& parent_predictives);
  int Sample(int current);
  
 private:
  std::vector<double> pdf_;
  bool include_zero_;
  std::vector<std::unique_ptr<SeatingArrangementProbCalculator> > arrangement_part_;
  std::unique_ptr<TopicPriorCalculator> topic_part_;
  
};

// class FloorSampler {
//  public:
//   FloorSampler(size_t num_topics, bool include_zero)
//       : pdf_(num_topics + 1, 0.0),
//         include_zero_(include_zero) {}

//   void ClearPdf() { fill(pdf_.begin(), pdf_.end(), 0.0); }
//   void ResetArrangementCache(const std::vector<double>& depth2discounts,
//                              const std::vector<double>& depth2concentrations) {
//     depth2discounts_ = depth2discounts;
//     depth2concentrations_ = depth2concentrations;
//   }
//   void ResetTopicPriorCache(const std::vector<double>& topic2alpha) {
//     topic2alpha_ = topic2alpha;
//     alpha_sum_ = std::accumulate(topic2alpha.begin(), topic2alpha.end(), 0.0);
//   }

//   void TakeInSectionLL(
//       const boost::container::flat_map<topic_t, std::pair<int, int> >& floor2c_t,
//       int add_customers,
//       int add_tables,
//       int topic,
//       int depth) {
//     int s = pdf_.size();
//     double discount = depth2discounts_[depth];
//     double concentration = depth2concentrations_[depth];
//     auto it = floor2c_t.begin();
//     int j = 0;
//     if (!include_zero_) {
//       j = 1;
//       if (it != floor2c_t.end() && (*it).first == 0) ++it;
//     }
//     for (; j < s; ++j) {
//       if (it == floor2c_t.end() || (*it).first > j) {
//         pdf_[j] += lgamma(concentration) - lgamma(concentration + add_customers);
//         for (int i = 0; i < add_tables; ++i) {
//           pdf_[j] += log(concentration + discount * i);
//         }
//       } else {
//         assert((*it).first == j);
//         int c = (*it).second.first;
//         int t = (*it).second.second;
//         if (j == topic) {
//           c -= add_customers;
//           t -= add_tables;
//         }
//         pdf_[j] += lgamma(concentration + c) - lgamma(concentration + c + add_customers);
//         for (int i = 0; i < add_tables; ++i) {
//           pdf_[j] += log(concentration + discount * (t + i));
//         }
//         ++it;
//       }
//     }
//   }
//   void TakeInLambda(double global_lambda, double local_lambda) {
//     pdf_[0] += log(global_lambda);
//     for (size_t j = 1; j < pdf_.size(); ++j) {
//       pdf_[j] += log(local_lambda);
//     }
//   }
//   void TakeInParentPredictive(const std::vector<double>& parent_predictives) {
//     for (size_t i = 0; i < pdf_.size(); ++i) {
//       pdf_[i] += log(parent_predictives[i]);
//     }
//   }
//   void TakeInPrior(int current_floor,
//                    const std::unordered_map<int, int>& doc2move_customers,
//                    const std::vector<std::pair<int, std::vector<int> > >& doc2topic_count) {
//     for (auto& doc2move_customer : doc2move_customers) {
//       int j = doc2move_customer.first;
//       int s = pdf_.size();
//       for (int k = 1; k < s; ++k) {
//         double alpha = topic2alpha_[k];
//         if (alpha == 0) continue;
//         int b_j = doc2move_customer.second;
//         double N_j = doc2topic_count[j].first + alpha_sum_ - b_j;
//         double n_jk = doc2topic_count[j].second[k] + alpha;
//         if (k == current_floor) {
//           n_jk -= b_j;
//           assert(n_jk >= 0);
//         }
//         pdf_[k] += lgamma(N_j) - lgamma(N_j + b_j) + lgamma(n_jk + b_j) - lgamma(n_jk);
//       }
//     }
//   }
//   int Sample(bool include_zero = false) {
//     for (size_t i = 0; i < pdf_.size(); ++i) {
//       if (pdf_[i] == 0) {
//         pdf_[i] = -INFINITY;
//       }
//     }
//     if (!include_zero_) pdf_[0] = -INFINITY;
//     normalize_logpdf(pdf_.begin(), pdf_.end());
//     return random->SampleUnnormalizedPdfRef(pdf_);
//   }

//  private:
//   std::vector<double> pdf_;
//   bool include_zero_;
//   std::vector<double> depth2discounts_;
//   std::vector<double> depth2concentrations_;
//   std::vector<double> topic2alpha_;
//   double alpha_sum_;

// };




};


#endif /* _HPY_LDA_FLOOR_SAMPLER_HPP_ */
