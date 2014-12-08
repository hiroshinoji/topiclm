#ifndef _HPY_LDA_PARTICLE_FILTER_SAMPLER_HPP_
#define _HPY_LDA_PARTICLE_FILTER_SAMPLER_HPP_

#include <vector>
#include <ostream>

namespace hpy_lda {

class HpyLdaSampler;
class ParticleFilterDocumentManager;
struct Word;

class ParticleFilterSampler {
 public:
  ParticleFilterSampler(HpyLdaSampler& hpy_lda_sampler,
                        ParticleFilterDocumentManager& pf_dmanager);
  ~ParticleFilterSampler();

  double Run(int step, std::ostream& os);
  
 private:
  void Resample(int current_idx);
  void CalcCurrentPredictives(const Word& word);
  double SampleTopic(int current_idx);
  
  HpyLdaSampler& hpy_lda_sampler_;
  ParticleFilterDocumentManager& pf_dmanager_;
  const int num_particles_;
  
  std::vector<std::vector<std::vector<double> > > doc_predictives_;
  std::vector<std::vector<double> > doc_stop_priors_;
  std::vector<std::vector<double> > doc_lambda_paths_;
  std::vector<int> doc_word_depths_;
};

} // hpy_lda

#endif /* _HPY_LDA_PARTICLE_FILTER_SAMPLER_HPP_ */
