#ifndef _HPY_LDA_CACHED_HPY_LDA_HPP_
#define _HPY_LDA_CACHED_HPY_LDA_HPP_

#include <vector>
#include <memory>
#include <pficommon/data/intern.h>
#include "serialization.hpp"
#include "context_tree_manager.hpp"
#include "config.hpp"

namespace hpy_lda {

class TopicDepthSamplerInterface;
class Parameters;
class DocumentManager;
struct SamplingConfiguration;
struct Word;

class CachedHpyLdaSampler {
  friend class CachedParticleFilterSampler;
 public:
  CachedHpyLdaSampler(LambdaType lambda_type, TreeType tree_type, DocumentManager& dmanager, Parameters& parameters);
  ~CachedHpyLdaSampler();

  void InitializeInRandom(int init_depth, bool hpy_random);
  void RunOneIteration(int iteration_i);
  
  //ParticleFilterSampler GetParticleFilterSampler(ParticleFilterDocumentManager& pf_dmanager);
  ContextTreeAnalyzer GetCTAnalyzer();

 private:
  DocumentManager& dmanager_;
  Parameters& parameters_;
  ContextTreeManager cmanager_;
  std::unique_ptr<TopicDepthSamplerInterface> topic_sampler_;
  std::vector<int> sampling_idxs_;
  LambdaType lambda_type_;
  TreeType tree_type_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(cmanager_);
  }
};

} // hpy_lda

#endif /* _HPY_LDA_CACHED_HPY_LDA_HPP_ */
