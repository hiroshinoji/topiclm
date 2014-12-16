#ifndef _TOPICLM_PARTICLE_FILTER_SAMPLER_HPP_
#define _TOPICLM_PARTICLE_FILTER_SAMPLER_HPP_

#include <vector>
#include <ostream>

namespace topiclm {

class HpyLdaSampler;
class ParticleFilterDocumentManager;
struct Word;

class ParticleFilterSampler {
 public:
  ParticleFilterSampler(HpyLdaSampler& hpy_lda_sampler,
                        ParticleFilterDocumentManager& pf_dmanager,
                        int step);
  ~ParticleFilterSampler();

  double Run(std::ostream& os);

  void ResampleAll();
  void Reset();
  
  double log_probability(const std::vector<int>& sentence, bool store = true);
  
 private:
  void Resample(int current_idx);
  void CalcCurrentPredictives(const Word& word);
  double SampleTopic(int current_idx);
  
  HpyLdaSampler& hpy_lda_sampler_;
  ParticleFilterDocumentManager& pf_dmanager_;
  const int num_particles_;
  const int step_;
  
  std::vector<std::vector<std::vector<double> > > doc_predictives_;
  std::vector<std::vector<double> > doc_stop_priors_;
  std::vector<std::vector<double> > doc_lambda_paths_;
  std::vector<int> doc_word_depths_;

  // local cache for one sentence
  std::vector<std::vector<int> > particle2sampled_topics_;
  std::vector<int> sentence_word_depths_;
  
  // std::vector<std::vector<double> > sentence_predictive_;
  
};

} // topiclm

#endif /* _TOPICLM_PARTICLE_FILTER_SAMPLER_HPP_ */
