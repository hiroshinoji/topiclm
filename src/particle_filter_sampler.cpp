#include <iostream>
#include <cassert>
#include "hpy_lda.hpp"
#include "document_manager.hpp"
#include "particle_filter_sampler.hpp"
#include "particle_filter_document_manager.hpp"
#include "topic_sampler.hpp"
#include "util.hpp"
#include "restaurant_manager.hpp"

using namespace std;

namespace hpy_lda {

ParticleFilterSampler::ParticleFilterSampler(HpyLdaSampler& hpy_lda_sampler,
                                             ParticleFilterDocumentManager& pf_dmanager)
    : hpy_lda_sampler_(hpy_lda_sampler),
      pf_dmanager_(pf_dmanager),
      num_particles_(pf_dmanager.num_particles()) {}

ParticleFilterSampler::~ParticleFilterSampler() {}

double ParticleFilterSampler::Run(int step, std::ostream& os) {
  double ll = 0;
  int num_samples = 0;
  assert(step > 0);
  int num_docs = pf_dmanager_.num_docs();
  for (int d = 0; d < num_docs; ++d) {
    cerr << setw(2) << d << "/" << num_docs << "\r";
    pf_dmanager_.SetCurrentDoc(d);
    os << "doc" << d << ":" << endl;

    doc_predictives_.clear();
    doc_stop_priors_.clear();
    doc_lambda_paths_.clear();
    doc_word_depths_.clear();
    
    int num_words = pf_dmanager_.doc_num_words();
    for (int i = 0; i < num_words; ++i) {
      if (i % step == 0) {
        Resample(i);
      }
      auto& word = pf_dmanager_.word(0, i);
      CalcCurrentPredictives(word);
      
      double p_w = SampleTopic(i);
      ll += log(p_w);
      ++num_samples;
      //os << getWord(pf_dmanager_.token(word), pf_dmanager_.intern()) << "\t" << p_w << endl;
      os << getWord(pf_dmanager_.token(word), pf_dmanager_.intern()) << "\t" << p_w << "\t" << doc_lambda_paths_[i][doc_word_depths_[i]] << endl;
    }
  }
  return exp(-ll / num_samples);
}
void ParticleFilterSampler::Resample(int current_idx) {
  for (int i = 0; i < current_idx; ++i) {
    for (int m = 0; m < num_particles_; ++m) {
      auto& word = pf_dmanager_.word(m, i);
      int topic = pf_dmanager_.topic(m, word);
      int word_depth = doc_word_depths_[i];
      pf_dmanager_.DecrementTopicCount(m, topic, word.is_general);

      hpy_lda_sampler_.topic_sampler_->InitWithTopicPrior(pf_dmanager_.topic_count(m),
                                                          doc_lambda_paths_[i],
                                                          word_depth);
      hpy_lda_sampler_.topic_sampler_->TakeInStopPrior(doc_stop_priors_[i]);
      hpy_lda_sampler_.topic_sampler_->TakeInLikelihood(doc_predictives_[i]);
      
      auto sample = hpy_lda_sampler_.topic_sampler_->Sample();
      if (hpy_lda_sampler_.ConsiderGeneral()) {
        word.is_general = sample.topic == 0;
      }
      pf_dmanager_.IncrementTopicCount(m, sample.topic, word.is_general);
      pf_dmanager_.set_topic(m, word, sample.topic);
    }
  }
}
void ParticleFilterSampler::CalcCurrentPredictives(const Word& word) {
  auto& sent = pf_dmanager_.sentence(word);
  int type = pf_dmanager_.token(word);
  int word_depth = hpy_lda_sampler_.cmanager_.WalkTreeNoCreate(sent, word.token_idx - 1, 0);
  hpy_lda_sampler_.cmanager_.CalcTestStopPriorPath(word_depth);
  hpy_lda_sampler_.cmanager_.rmanager().CalcDepth2TopicPredictives(type, word_depth, true);
  auto& predictives = hpy_lda_sampler_.cmanager_.rmanager().depth2topic_predictives();
  
  doc_predictives_.push_back(predictives);
  doc_stop_priors_.push_back(hpy_lda_sampler_.cmanager_.stop_prior_path());
  doc_lambda_paths_.push_back(hpy_lda_sampler_.cmanager_.lambda_path());
  doc_word_depths_.push_back(word_depth);
}
double ParticleFilterSampler::SampleTopic(int current_idx) {
  assert(current_idx == (int)doc_predictives_.size() - 1);
  double p_w = 0;
  for (int m = 0; m < num_particles_; ++m) {
    auto& word_m = pf_dmanager_.word(m, current_idx);
    int word_depth = doc_word_depths_[current_idx];

    hpy_lda_sampler_.topic_sampler_->InitWithTopicPrior(pf_dmanager_.topic_count(m),
                                                        doc_lambda_paths_[current_idx],
                                                        word_depth);
    hpy_lda_sampler_.topic_sampler_->TakeInStopPrior(doc_stop_priors_[current_idx]);
    hpy_lda_sampler_.topic_sampler_->TakeInLikelihood(doc_predictives_[current_idx]);
    
    auto sample = hpy_lda_sampler_.topic_sampler_->Sample();
    if (hpy_lda_sampler_.ConsiderGeneral()) {
      word_m.is_general = sample.topic == 0;
    }
    p_w += sample.p_w;

    pf_dmanager_.IncrementTopicCount(m, sample.topic, word_m.is_general);
    pf_dmanager_.set_topic(m, word_m, sample.topic);
  }
  p_w /= num_particles_;
  assert(p_w <= 1.0);
  return p_w;
}

} // hpy_lda
