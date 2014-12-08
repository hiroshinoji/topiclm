#include "dirichlet_sampler.hpp"
#include "random_util.hpp"
#include "parameters.hpp"

using namespace hpy_lda;
using namespace std;

int main(int /*argc*/, char **/*argv*/)
{
  init_rnd(0);
  int num_docs = 1000;
  int doc_len = 1000;

  vector<double> alphas = {0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1};
  int num_topics = alphas.size() - 1;

  vector<pair<int, vector<int> > > doc2topic_counts(num_docs);
  for (int i = 0; i < num_docs; ++i) {
    vector<double> topic_dist = hpy_lda::random->NextDirichlet(alphas);
    doc2topic_counts[i].second.resize(num_topics + 1);
    for (int j = 0; j < doc_len; ++j) {
      int topic = hpy_lda::random->SampleUnnormalizedPdf(topic_dist);
      doc2topic_counts[i].second[topic]++;
      doc2topic_counts[i].first++;
    }
  }

  DirichletParameter parameter(10, 10, num_topics);
  UniformDirichletSampler alpha_sampler;
  //GlobalSpecializedDirichletSampler alpha_sampler;
  //NonUniformDirichletSampler alpha_sampler;

  for (int i = 0; i < 1000; ++i) {
    alpha_sampler.Update(doc2topic_counts, parameter);
    for (size_t i = 0; i < parameter.alpha.size(); ++i) {
      cout << parameter.alpha[i] << " ";
    }
    cout << endl;
  }
  
  return 0;
}
