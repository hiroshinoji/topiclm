#include "parameters.hpp"
#include "context_tree.hpp"
#include "random_util.hpp"

using namespace std;

namespace hpy_lda {

double CalcLambdaCPosterior(
    const std::set<Node*>& some_depth_nodes,
    double c,
    double gamma_a,
    double gamma_b);

Parameters::Parameters() {}
Parameters::Parameters(double lambda_a,
                       double lambda_b,
                       double ini_lambda_c,
                       double dhpy_prior_pass,
                       double dhpy_prior_stop,
                       double ini_discount,
                       double ini_concentration,
                       double alpha_0,
                       double alpha_1,
                       int num_topics,
                       int ngram_order)
    : hpy_parameter_(num_topics,
                     ngram_order,
                     ini_discount,
                     ini_concentration,
                     dhpy_prior_pass,
                     dhpy_prior_stop),
      lambda_parameter_(lambda_a, lambda_b, ini_lambda_c, ngram_order),
      topic_parameter_(alpha_0, alpha_1, num_topics),
      ngram_order_(ngram_order) {}

Parameters::~Parameters() = default;

void Parameters::SetAlphaSampler(HyperSamplerType sampler_type) {
  if(sampler_type == kUniform) {
    alpha_sampler_.reset(new UniformDirichletSampler());
  } else if (sampler_type == kNonUniform) {
    alpha_sampler_.reset(new NonUniformDirichletSampler());
  } else {
    throw "not yet supported alpha sampler!";
  }
}
void Parameters::SamplingAlpha(
    const std::vector<std::pair<int, std::vector<int> > >& doc2topic_counts,
    const std::vector<std::vector<std::vector<int> > >& doc2topic2tables) {
  alpha_sampler_->UpdateBeta(doc2topic2tables, topic_parameter_);  
  alpha_sampler_->Update(doc2topic_counts, topic_parameter_);
}
void Parameters::SamplingLambdaConcentration(
    const std::vector<std::set<Node*> >& depth2wnodes) {
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    double posterior
        = CalcLambdaCPosterior(depth2wnodes[i], lambda_parameter_.c[i], 1.0, 1.0);
    lambda_parameter_.c[i] = posterior;
  }
}
void Parameters::SetHpySampler(HyperSamplerType sampler_type) {
  if (sampler_type == kUniform) {
    hpy_sampler_.reset(new UniformHpySampler(topic_parameter_.num_topics));
  } else if (sampler_type == kNonUniform) {
    hpy_sampler_.reset(new NonUniformHpySampler(topic_parameter_.num_topics));
  } else {
    throw "not yet supported hpy sampler!";
  }
}
void Parameters::SamplingHpyParameter(
    const std::vector<std::set<Node*> >& depth2wnodes) {
  hpy_sampler_->Update(depth2wnodes, hpy_parameter_);
}

string Parameters::OutputHypers() {
  stringstream ss;
  ss << "hpy parameters: discount, concentration" << std::endl;
  for (int j = 0; j < ngram_order_; ++j) {
    ss << "[" << j << "] ";// << hpy_parameter_.discount(j, 0) << ", " << hpy_parameter_.concentration(j, 0) << endl;
    for (int i = 0; i < min(3, topic_parameter_.num_topics + 1); ++i) {
      ss << "(" << hpy_parameter_.discount(j, i) << ", " << hpy_parameter_.concentration(j, i) << ") ";
    }
    ss << endl;
  }
  ss << "cache: " << hpy_parameter_.cache_discount(0) << ", " << hpy_parameter_.cache_concentration(0) << endl;
  ss << "betas" << endl;
  for (auto& b_k : topic_parameter_.beta) ss << b_k << " ";
  ss << endl;
  ss << "alpha_0: " << topic_parameter_.alpha_0 << endl;
  ss << "alpha_1: " << topic_parameter_.alpha_1 << endl;
  for (int j = 0; j < ngram_order_; ++j) {
    ss << "[" << j << "] " << lambda_parameter_.c[j] << endl;
  }
  return ss.str();
}

double CalcLambdaCPosterior(
    const std::set<Node*>& some_depth_nodes,
    double c,
    double gamma_a,
    double gamma_b) {
  double s_d = 0;
  double log_w = 0;
  double m_dot = 0;

  for (auto node : some_depth_nodes) {
    auto& restaurant = node->restaurant();
    int local_tables = restaurant.local_labeled_tables();
    int global_tables = restaurant.global_labeled_tables();
    int sum_tables = local_tables + global_tables;
    s_d += random->NextBernoille((sum_tables / c) / (sum_tables / c + 1));
    log_w += log(random->NextBeta(c + 1, sum_tables));
    
    m_dot += restaurant.local_labeled_table_tables();
    m_dot += restaurant.global_labeled_table_tables();
  }
  return random->NextGamma(gamma_a - s_d + m_dot, gamma_b - log_w);
}


};
