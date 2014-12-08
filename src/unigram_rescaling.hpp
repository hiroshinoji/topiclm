#ifndef _HPY_LDA_UNIGRAM_RESCALING_H_
#define _HPY_LDA_UNIGRAM_RESCALING_H_

#include <vector>
#include <memory>
#include <pficommon/data/intern.h>
#include "serialization.hpp"
#include "context_tree_manager.hpp"
#include "config.hpp"
#include "particle_filter_sampler.hpp"
#include "topic_sampler.hpp"

namespace hpy_lda {

class Parameters;
class DocumentManager;
class ParticleFilterDocumentManager;

class UnigramRescalingSampler {
  friend class UnigramParticleFilterSampler;
 public:
  UnigramRescalingSampler(LambdaType lambda_type, TreeType tree_type, DocumentManager& dmanager, Parameters& parameters);
  ~UnigramRescalingSampler();

  void SetBeta(double beta) {
    beta_ = beta;
    beta_sum_ = beta_ * unigram_counts_.size();
  }
  void InitializeInRandom();
  void RunOneIteration(int iteration_i);

  double PredictInParticleFilter(ParticleFilterDocumentManager& pf_dmanager,
                                 int num_particles,
                                 int step,
                                 double rescale_factor,
                                 std::ostream& os);
  ContextTreeAnalyzer GetCTAnalyzer();
  
 private:
  void SampleLdaPart(int iteration_i);
  void SampleHpyPart(int iteration_i);
  
  void CalcTopic2WordProb(int type);
  double UnigramWordProb(int type) const;
  double HpyUnigramWordProb(int type);
  void UpdateBeta();

  DocumentManager& dmanager_;
  Parameters& parameters_;
  ContextTreeManager cmanager_;
  TopicSampler topic_sampler_;
  std::vector<int> sampling_idxs_;

  std::vector<double> topic2word_prob_;
  std::vector<int> unigram_counts_;
  int unigram_sum_count_;
  std::vector<std::pair<int, std::unordered_map<int, int> > > topic2word_counts_;
  double beta_;
  double beta_sum_;

  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(cmanager_)
        & MEMBER(topic2word_prob_)
        & MEMBER(unigram_counts_)
        & MEMBER(unigram_sum_count_)
        & MEMBER(topic2word_counts_)
        & MEMBER(beta_);
    if (ar.is_read) {
      beta_sum_ = beta_ * unigram_counts_.size();
    }
  }
  
};

} // hpy_lda

#endif /* _HPY_LDA_UNIGRAM_RESCALING_H_ */





