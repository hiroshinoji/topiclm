#ifndef _TOPICLMPARAMETERS_HPP_
#define _TOPICLMPARAMETERS_HPP_

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include "serialization.hpp"
#include "dirichlet_sampler.hpp"
#include "hpy_sampler.hpp"

namespace topiclm {

class Node;

enum HyperSamplerType { kUniform = 0, kNonUniform, kGlobalSpecialized };

class HPYParameter {
 public:
  HPYParameter() {}
  HPYParameter(int num_topics,
               int ngram_order,
               double discount,
               double concentration,
               double prior_pass,
               double prior_stop)
      : topic_depth2discounts(num_topics + 1,
                              std::vector<double>(ngram_order, discount)),
        topic_depth2concentrations(num_topics + 1,
                                   std::vector<double>(ngram_order, concentration)),
        depth2cache_discounts(ngram_order, discount),
        depth2cache_concentrations(ngram_order, concentration),
        prior_pass_(prior_pass),
        prior_stop_(prior_stop) {}
  double discount(int depth, int floor_id) const {
    return topic_depth2discounts[floor_id][depth];
  }
  double concentration(int depth, int floor_id) const {
    return topic_depth2concentrations[floor_id][depth];
  }
  double cache_discount(int depth) const {
    return depth2cache_discounts[depth];
  }
  double cache_concentration(int depth) const {
    return depth2cache_concentrations[depth];
  }
  double prior_pass() const {
    return prior_pass_;
  }
  double prior_stop() const {
    return prior_stop_;
  }
  const std::vector<double>& depth2discount() const { return topic_depth2discounts[0]; }
  const std::vector<double>& depth2concentration() const { return topic_depth2concentrations[0]; }
  
  void set_discount(int depth, int floor_id, double discount) {
    topic_depth2discounts[floor_id][depth] = discount;
  }
  void set_cache_discount(int depth, double discount) {
    depth2cache_discounts[depth] = discount;
  }
  void set_cache_concentration(int depth, double concentration) {
    depth2cache_concentrations[depth] = concentration;
  }
  void set_concentration(int depth, int floor_id, double concentration) {
    topic_depth2concentrations[floor_id][depth] = concentration;
  }
 private:
  std::vector<std::vector<double> > topic_depth2discounts;
  std::vector<std::vector<double> > topic_depth2concentrations;
  std::vector<double> depth2cache_discounts;
  std::vector<double> depth2cache_concentrations;
  
  double prior_pass_;
  double prior_stop_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(topic_depth2discounts)
        & MEMBER(topic_depth2concentrations)
        & MEMBER(depth2cache_discounts)
        & MEMBER(depth2cache_concentrations)
        & MEMBER(prior_pass_)
        & MEMBER(prior_stop_);
  }
};

struct LambdaParameter {
  LambdaParameter() {}
  LambdaParameter(double a, double b, double c, int ngram_order)
      : a(a), b(b), c(ngram_order, c) {}
  double a;
  double b;
  std::vector<double> c;
  
  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(a)
        & MEMBER(b)
        & MEMBER(c);
  }
};

struct DirichletParameter {
  DirichletParameter() {}
  DirichletParameter(double alpha_0, double alpha_1, int num_topics)
      : alpha(num_topics + 1, 0),
        beta(num_topics + 1, 0),
        alpha_0(alpha_0),
        alpha_1(alpha_1),
        num_topics(num_topics) {
    for (size_t i = 1; i < alpha.size(); ++i) {
      beta[i] = 1.0 / num_topics;
      alpha[i] = alpha_1 * beta[i];
      assert(alpha[i] >= 0);
    }
  }
  std::vector<double> alpha; // used as a topic prior (alpha_1 * beta)
  std::vector<double> beta; // a draw from Dir(alpha0)
  double alpha_0;
  double alpha_1;

  int num_topics;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(alpha)
        & MEMBER(beta)
        & MEMBER(alpha_0)
        & MEMBER(alpha_1)
        & MEMBER(num_topics);
  }
};

class Parameters {
 public:
  Parameters();
  Parameters(double lambda_a,
             double lambda_b,
             double ini_lambda_c,
             double dhpy_piror_pass,
             double dhpy_prior_stop,
             double ini_discount,
             double ini_concentration,
             double alpha_0,
             double alpha_1,
             int num_topics,
             int ngram_order);
  Parameters(Parameters&&) = default;
  ~Parameters();

  void SetAlphaSampler(HyperSamplerType sampler_type);
  void SetHpySampler(HyperSamplerType sampler_type);
  void SamplingAlpha(
      const std::vector<std::pair<int, std::vector<int> > >& doc2topic_counts,
      const std::vector<std::vector<std::vector<int> > >& doc2topic2tables);
  void SamplingLambdaConcentration(
      const std::vector<std::set<Node*> >& depth2wnodes);
  void SamplingHpyParameter(
      const std::vector<std::set<Node*> >& depth2wnodes);

  std::string OutputHypers();
  
  const HPYParameter& hpy_parameter() const { return hpy_parameter_; }
  const LambdaParameter& lambda_parameter() const { return lambda_parameter_; }
  const DirichletParameter& topic_parameter() const { return topic_parameter_; }
  int ngram_order() const { return ngram_order_; }
 private:
  std::unique_ptr<DirichletSamplerInterface> alpha_sampler_;
  std::unique_ptr<HpySamplerInterface> hpy_sampler_;
  HPYParameter hpy_parameter_;
  LambdaParameter lambda_parameter_;
  DirichletParameter topic_parameter_;
  int ngram_order_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(hpy_parameter_)
      & MEMBER(lambda_parameter_)
      & MEMBER(topic_parameter_)
      & MEMBER(ngram_order_);
  }
};

} // namespace topiclm

#endif /* _TOPICLMPARAMETERS_HPP_ */
