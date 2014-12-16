#include "parameters.hpp"
#include "dirichlet_sampler.hpp"
#include "random_util.hpp"

using namespace std;

namespace topiclm {

void UniformDirichletSampler::Update(
    const vector<pair<int, vector<int> > >& doc2topic_counts,
    DirichletParameter& topic_parameter) {
  double a = 1.0, b = 1.0;
  double s_d = 0;
  double z_kj_inv = 0;
  double log_w = 0;

  double alpha_1 = topic_parameter.alpha_1;
  for (auto& topic_count : doc2topic_counts) {
    assert(topic_count.second.size() == topic_parameter.alpha.size());
    if (topic_count.first == 0) continue;
    s_d += random->NextBernoille(
        (topic_count.first / alpha_1) / (topic_count.first / alpha_1 + 1));
    log_w += log(random->NextBeta(alpha_1 + 1, topic_count.first));
    for (size_t i = 0; i < topic_count.second.size(); ++i) {
      if (topic_parameter.alpha[i] == 0) continue;
      int n_dk = topic_count.second[i];
      if (n_dk >= 1) {
        z_kj_inv += 1;
        for (int j = 1; j < n_dk; ++j) {
          z_kj_inv += 1 - random->NextBernoille(j / (j + topic_parameter.alpha[i]));
        }
      }
    }
  }
  topic_parameter.alpha_1 = random->NextGamma(a - s_d + z_kj_inv, b - log_w);
  for (size_t i = 1; i < topic_parameter.alpha.size(); ++i) {
    topic_parameter.alpha[i] = topic_parameter.alpha_1 * topic_parameter.beta[i];
  }
}
void NonUniformDirichletSampler::UpdateBeta(
      const std::vector<std::vector<std::vector<int> > >& doc2topic2tables,
      DirichletParameter& topic_parameter) {
  auto& beta = topic_parameter.beta;
  double alpha_0 = topic_parameter.alpha_0;
  double alpha_k = alpha_0 / topic_parameter.num_topics;
  
  vector<double> table_sum_counts(topic_parameter.num_topics + 1, 0);
  for (auto& doc_topic2tables : doc2topic2tables) {
    assert(doc_topic2tables[0].empty());
    for (size_t i = 1; i < doc_topic2tables.size(); ++i) {
      table_sum_counts[i] += doc_topic2tables[i].size();
    }
  }
  for (size_t i = 1; i < table_sum_counts.size(); ++i) {
    table_sum_counts[i] += alpha_k;
  }
  beta = random->NextDirichlet(table_sum_counts);
  
  assert((int)beta.size() == topic_parameter.num_topics + 1);
  assert(beta[0] == 0);

  double a = 1.0, b = 1.0;
  double s = 0;
  double z_k_inv = 0;
  double log_w = 0;

  int sum_tables = accumulate(table_sum_counts.begin(), table_sum_counts.end(), 0);
  s += random->NextBernoille((sum_tables / alpha_0) / (sum_tables / alpha_0 + 1));
  log_w += log(random->NextBeta(alpha_0 + 1, sum_tables));
  for (size_t i = 0; i < table_sum_counts.size(); ++i) {
    if (int m_k = table_sum_counts[i]) {
      z_k_inv += 1;
      for (int j = 1; j < m_k; ++j) {
        z_k_inv += 1 - random->NextBernoille(j / (j + alpha_k));
      }
    }
  }
  topic_parameter.alpha_0 = random->NextGamma(a - s + z_k_inv, b - log_w);
}

// void NonUniformDirichletSampler::Update(
//     const vector<pair<int, vector<int> > >& doc2topic_counts,
//     DirichletParameter& topic_parameter) {
//   double a = 1.0, b = 1.0;

//   for (size_t i = 0; i < topic_parameter.alpha.size(); ++i) {
//     if (topic_parameter.alpha[i] == 0) continue;
//     double s_d = 0;
//     double log_w = 0;
//     double z_kj_inv = 0;
//     double alpha_sum = topic_parameter.alpha_sum;
//     double alpha = topic_parameter.alpha[i];
    
//     for (auto& topic_count : doc2topic_counts) {
//       assert(topic_count.second.size() == topic_parameter.alpha.size());
//       //s_d += random->NextBernoille(alpha / (alpha_sum + topic_count.first));
//       log_w += log(random->NextBeta(alpha_sum, topic_count.first));
//       int n_dk = topic_count.second[i];
//       if (n_dk >= 1) {
//         z_kj_inv += 1;
//         for (int j = 1; j < n_dk; ++j) {
//           z_kj_inv += 1 - random->NextBernoille(j / (j + alpha));
//         }
//       }
//     }
//     if (z_kj_inv == 0) continue;
//     double new_alpha = random->NextGamma(a + s_d + z_kj_inv, b - log_w);
//     topic_parameter.alpha_sum += (new_alpha - alpha);
//     topic_parameter.alpha[i] = new_alpha;
//   }
//   topic_parameter.alpha_sum = accumulate(topic_parameter.alpha.begin(), topic_parameter.alpha.end(), 0.0);
// }

// void GlobalSpecializedDirichletSampler::Update(
//  const vector<pair<int, vector<int> > >& doc2topic_counts,
//     DirichletParameter& topic_parameter) {
//   double a = 1.0, b = 1.0;

//   {
//     double log_w = 0;
//     double z_kj_inv = 0;
//     double alpha_sum = topic_parameter.alpha_sum;
//     double alpha = topic_parameter.alpha[0];
//     assert(alpha != 0);
    
//     for (auto& topic_count : doc2topic_counts) {
//       log_w += log(random->NextBeta(alpha_sum, topic_count.first));
//       int n_dk = topic_count.second[0];
//       if (n_dk >= 1) {
//         z_kj_inv += 1;
//         for (int j = 1; j < n_dk; ++j) {
//           z_kj_inv += 1 - random->NextBernoille(j / (j + alpha));
//         }
//       }
//     }
//     if (z_kj_inv > 0) {
//       double new_alpha = random->NextGamma(a + z_kj_inv, b - log_w);
//       topic_parameter.alpha_sum += (new_alpha - alpha);
//       topic_parameter.alpha[0] = new_alpha;
//     }
//   }
//   double log_w = 0;
//   double z_kj_inv = 0;
//   double alpha_sum = topic_parameter.alpha_sum;
//   double alpha = topic_parameter.alpha[1];
  
//   for (auto& topic_count : doc2topic_counts) {
//     //log_w += topic_num * log(random->NextBeta(alpha_sum, topic_count.first));
//     for (size_t i = 1; i < topic_count.second.size(); ++i) {
//       log_w += log(random->NextBeta(alpha_sum, topic_count.first));
//     }
//     for (size_t i = 1; i < topic_count.second.size(); ++i) {
//       int n_dk = topic_count.second[i];
//       if (n_dk >= 1) {
//         z_kj_inv += 1;
//         for (int j = 1; j < n_dk; ++j) {
//           z_kj_inv += 1 - random->NextBernoille(j / (j + alpha));
//         }
//       }
//     }
//   }
//   double new_alpha = random->NextGamma(a + z_kj_inv, b - log_w);
//   for (size_t i = 1; i < topic_parameter.alpha.size(); ++i) {
//     topic_parameter.alpha[i] = new_alpha;
//   }
//   topic_parameter.alpha_sum = accumulate(topic_parameter.alpha.begin(), topic_parameter.alpha.end(), 0.0);
// }

} // topiclm
