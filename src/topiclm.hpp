#ifndef _TOPICLM_H_
#define _TOPICLM_H_

#include <vector>
#include <memory>
#include <pficommon/data/intern.h>
#include "serialization.hpp"
#include "context_tree_manager.hpp"
#include "config.hpp"
#include "particle_filter_sampler.hpp"

namespace topiclm {

class TopicDepthSampler;
class Parameters;
class DocumentManager;
class ParticleFilterDocumentManager;
struct SamplingConfiguration;
struct Word;

class HpyLdaSampler {
  friend class ParticleFilterSampler;
 public:
  HpyLdaSampler(LambdaType lambda_type, TreeType tree_type, DocumentManager& dmanager, Parameters& parameters);
  ~HpyLdaSampler();

  void InitializeInRandom(int init_depth, bool hpy_random, double p_global);
  double RunOneIteration(int iteration_i, bool table_sample);
  
  ParticleFilterSampler GetParticleFilterSampler(ParticleFilterDocumentManager& pf_dmanager, int step_size);
  ContextTreeAnalyzer GetCTAnalyzer();

  void set_table_based_sampler(int max_t_in_block, int max_c_in_block, bool include_root);

 private:
  bool ConsiderGeneral() {
    return tree_type_ == kNonGraphical;
  }
  double logjoint() const;
  
  DocumentManager& dmanager_;
  Parameters& parameters_;
  ContextTreeManager cmanager_;
  std::unique_ptr<TopicDepthSampler> topic_sampler_;
  std::vector<int> sampling_idxs_;
  LambdaType lambda_type_;
  TreeType tree_type_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(cmanager_);
  }
};


} // namespace topiclm

#endif /* _TOPICLM_H_ */
