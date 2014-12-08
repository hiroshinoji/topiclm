#ifndef _HPY_LDA_HPY_LDA_MODEL_HPP_
#define _HPY_LDA_HPY_LDA_MODEL_HPP_

#include <memory>
#include <cassert>
#include "parameters.hpp"
#include "hpy_lda.hpp"
#include "unigram_rescaling.hpp"
#include "config.hpp"
#include "serialization.hpp"
#include "document_manager.hpp"
#include "particle_filter_document_manager.hpp"
#include "cached_hpy_lda.hpp"

namespace hpy_lda {

template <class SamplerType>
class HpyLdaModel {
 public:
  HpyLdaModel() {}
  HpyLdaModel(HpyLdaModel&&) = default;
  HpyLdaModel(double lambda_a,
              double lambda_b,
              double lambda_c,
              double prior_pass,
              double prior_stop,
              double discount,
              double concentration,
              double alpha_0,
              double alpha_1,
              int num_topics,
              int ngram_order,
              LambdaType lambda_type,
              TreeType tree_type)
      : lambda_type_(lambda_type),
        tree_type_(tree_type),
        parameters_(lambda_a,
                    lambda_b,
                    lambda_c,
                    prior_pass,
                    prior_stop,
                    discount,
                    concentration,
                    alpha_0,
                    alpha_1,
                    num_topics,
                    ngram_order),
        dmanager_(num_topics, ngram_order) {}
  
  void ReadTrainFile(const std::string& fn) {
    dmanager_.Read(fn);
  }
  ParticleFilterDocumentManager GetPFDocumentManager(int num_particles) {
    return ParticleFilterDocumentManager(dmanager_.intern(),
                                         num_particles,
                                         parameters_.topic_parameter().num_topics,
                                         parameters_.ngram_order());
  }
  void SetSampler() {
    sampler_.reset(new SamplerType(lambda_type_, tree_type_, dmanager_, parameters_));
  }
  void SetAlphaSampler(HyperSamplerType sample_type) {
    parameters_.SetAlphaSampler(sample_type);
  }
  void SetHpySampler(HyperSamplerType sample_type) {
    parameters_.SetHpySampler(sample_type);
  }
  void SaveModels(const std::string& dir, int i) {
    std::string model_fn = dir + "/model/" + std::to_string(i) + ".out";
    SaveModel(model_fn, *this);
    std::string topic_prefix = dir + "/topic/" + std::to_string(i);
    std::string assign_fn = topic_prefix + ".assign";
    std::string count_fn = topic_prefix + ".count";
    {
      std::ofstream ofs(assign_fn);
      dmanager_.OutputTopicAssign(ofs);
    }
    {
      std::ofstream ofs(count_fn);
      dmanager_.OutputTopicCount(ofs);
    }
  }
  
  SamplerType& sampler() { return *sampler_; }
  
 private:
  LambdaType lambda_type_;
  TreeType tree_type_;
  std::unique_ptr<SamplerType> sampler_;
  Parameters parameters_;
  DocumentManager dmanager_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    if (ar.is_read) {
      int lambda_type;
      ar & lambda_type;      
      int tree_type;
      ar & tree_type;
      lambda_type_ = LambdaType(lambda_type);
      tree_type_ = TreeType(tree_type);
    } else {
      int lambda_type = int(lambda_type_);
      ar & lambda_type;
      int tree_type = int(tree_type_);
      ar & tree_type;
    }
    ar & MEMBER(parameters_)
        & MEMBER(dmanager_);
    if (ar.is_read) {
      SetSampler();
    }
    ar & MEMBER(*sampler_);
  }
};

template <class SamplerType>
inline void SaveModel(const std::string& fn, HpyLdaModel<SamplerType>& model) {
  std::ofstream ofs(fn);
  if (!ofs) {
    throw "cannot open model file " + fn;
  }
  pfi::data::serialization::binary_oarchive oa(ofs);
  oa << model;
  std::string end_flag = "end";
  oa << end_flag;
}

template <class SamplerType>
inline HpyLdaModel<SamplerType> LoadModel(const std::string& fn) {
  HpyLdaModel<SamplerType> model;
  std::ifstream ifs(fn);
  if (!ifs) {
    throw "cannot read model file " + fn;
  }
  pfi::data::serialization::binary_iarchive ia(ifs);

  ia >> model;

  std::string end_flag;
  ia >> end_flag;
  assert(end_flag == "end");
  return model;
}

} // hpy_lda
  
#endif /* _HPY_LDA_HPY_LDA_MODEL_HPP_ */
