#include <iostream>
#include <iomanip>
#include "util.hpp"
#include "random_util.hpp"
#include "unigram_rescaling.hpp"
#include "document_manager.hpp"
#include "particle_filter_document_manager.hpp"
#include "parameters.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"
#include "lambda_manager.hpp"
#include "restaurant_manager.hpp"

using namespace std;

namespace hpy_lda {

UnigramRescalingSampler::UnigramRescalingSampler(LambdaType lambda_type, TreeType tree_type, DocumentManager& dmanager, Parameters& parameters)
    : dmanager_(dmanager),
      parameters_(parameters),
      cmanager_(lambda_type, tree_type, parameters, dmanager.lexicon()),
      topic_sampler_(parameters),
      topic2word_prob_(parameters.topic_parameter().num_topics + 1),
      unigram_sum_count_(0),
      topic2word_counts_(parameters.topic_parameter().num_topics + 1),
      beta_(0) {
  int num_words = dmanager_.num_words();
  sampling_idxs_.clear();
  for (int i = 0; i < num_words; ++i) {
    sampling_idxs_.push_back(i);
  }
  unigram_counts_ = dmanager_.GetUnigramCounts();
  for (auto count : unigram_counts_) {
    unigram_sum_count_ += count;
  }
}

UnigramRescalingSampler::~UnigramRescalingSampler() {}

void UnigramRescalingSampler::InitializeInRandom() {
  assert(beta_ > 0);
  assert(sampling_idxs_.size() > 0);
  cerr << "initializting..." << endl;
  random_shuffle(sampling_idxs_.begin(), sampling_idxs_.end(), *random);
  for (size_t j = 0; j < sampling_idxs_.size(); ++j) {
    auto& word = dmanager_.word(j);
    auto& sent = dmanager_.sentence(*word);
    int type = dmanager_.token(*word);
    word->depth = cmanager_.WalkTree(sent, word->token_idx - 1, 0, parameters_.ngram_order() - 1);
    int sampled_topic = topic_sampler_.SampleAtRandom();
    
    ++topic2word_counts_[sampled_topic].second[type];
    ++topic2word_counts_[sampled_topic].first;
    
    dmanager_.IncrementTopicCount(word->doc_id, sampled_topic, parameters_.topic_parameter().alpha[sampled_topic]);
    dmanager_.set_topic(*word, sampled_topic);

    cmanager_.rmanager().CalcGlobalPredictivePath(type, word->depth);
    cmanager_.rmanager().AddCustomerToPath(type, 0, word->depth);

    word->node = cmanager_.current_node_path()[word->depth];
    assert(word->node != nullptr);
  }
}

void UnigramRescalingSampler::RunOneIteration(int iteration_i) {
  assert(beta_ > 0);
  random_shuffle(sampling_idxs_.begin(), sampling_idxs_.end(), *random);
  if (iteration_i == 0) {
    SampleHpyPart(iteration_i);
    SampleLdaPart(iteration_i);
  } else {
    if (iteration_i % 10 == 0) {
      SampleHpyPart(iteration_i);
    } else {
      SampleLdaPart(iteration_i);
    }
  }
}
double UnigramRescalingSampler::PredictInParticleFilter(
    ParticleFilterDocumentManager& pf_dmanager,
    int num_particles,
    int step,
    double rescale_factor,
    ostream& os) {
  cerr << "test set particle filter start ..." << endl;
  vector<double> current_priors(topic2word_prob_.size());
  double ll = 0;
  int num_samples = 0;
  assert(step > 0);
  int num_docs = pf_dmanager.num_docs();
  for (int d = 0; d < num_docs; ++d) {
    cerr << setw(2) << d << "/" << num_docs << "\r";
    pf_dmanager.SetCurrentDoc(d);
    os << "doc" << d << ":" << endl;

    vector<vector<double> > doc_predictives;
    int num_words = pf_dmanager.doc_num_words();
    for (int i = 0; i < num_words; ++i) {
      // resample
      if (i % step == 0) {
        for (int j = 0; j < i; ++j) {
          for (int m = 0; m < num_particles; ++m) {
            auto& word = pf_dmanager.word(m, j);
            int topic = pf_dmanager.topic(m, word);
            pf_dmanager.DecrementTopicCount(m, topic);
            topic_sampler_.InitWithTopicPrior(pf_dmanager.topic_count(m));
            topic_sampler_.TakeInLikelihood(doc_predictives[j]);
            auto sample = topic_sampler_.Sample();
            pf_dmanager.IncrementTopicCount(m, sample.topic);
            pf_dmanager.set_topic(m, word, sample.topic);
          }
        }
      } // end
      auto& word = pf_dmanager.word(0, i);
      auto& sent = pf_dmanager.sentence(word);
      int type = pf_dmanager.token(word);
      double p_w = 0;
      double normalize_constant = 0;

      int word_depth = cmanager_.WalkTreeNoCreate(sent, word.token_idx - 1, 0);

      fill(current_priors.begin(), current_priors.end(), 0.0);
      for (int m = 0; m < num_particles; ++m) {
        topic_sampler_.InitWithTopicPrior(pf_dmanager.topic_count(m));
        auto& prior = topic_sampler_.topic_pdf();
        assert(prior.size() == current_priors.size());
        for (size_t i = 0; i < prior.size(); ++i) {
          current_priors[i] += prior[i];
        }
      }
      // for debugging purpose (should be removed):
      vector<double> explicit_mdivided_prior = current_priors;
      for (size_t i = 0; i < explicit_mdivided_prior.size(); ++i) explicit_mdivided_prior[i] /= num_particles;
      double accum = std::accumulate(current_priors.begin(), current_priors.end(), 0.0);
      for (size_t i = 0; i < current_priors.size(); ++i) {
        current_priors[i] /= accum;
      }
      for (size_t i = 0; i < current_priors.size(); ++i) {
        assert(abs(current_priors[i] - explicit_mdivided_prior[i]) < 1e-3);
      }
      
      for (size_t j = 0; j < unigram_counts_.size(); ++j) {
        cmanager_.rmanager().CalcGlobalPredictivePath(j, word_depth);
        double p_u_hpy =  cmanager_.rmanager().depth2topic_predictives()[word_depth][0];
        
        double p_u_h = 0;
        CalcTopic2WordProb(j);
        
        for (size_t k = 0; k < current_priors.size(); ++k) {
          p_u_h += topic2word_prob_[k] * current_priors[k];
        }
        
        p_u_h /= UnigramWordProb(j);
        p_u_h = pow(p_u_h, rescale_factor);
        double p_u = p_u_h * p_u_hpy;
        normalize_constant += p_u;
        if ((int)j == type) {
          p_w = p_u;
        }
      }
      assert(p_w > 0);
      p_w /= normalize_constant;
      ll += log(p_w);
      ++num_samples;

      CalcTopic2WordProb(type); // calc topic2word_prob_: p(type|0), p(type|1), ...
      doc_predictives.push_back(topic2word_prob_);

      for (int m = 0; m < num_particles; ++m) {
        topic_sampler_.InitWithTopicPrior(pf_dmanager.topic_count(m));
        topic_sampler_.TakeInLikelihood(topic2word_prob_);
        auto sample = topic_sampler_.Sample();
        pf_dmanager.IncrementTopicCount(m, sample.topic);
        pf_dmanager.set_topic(m, word, sample.topic);
      }
      os << getWord(type, pf_dmanager.intern()) << "\t" << p_w << endl;
    }
  }
  cerr << "done" << endl;
  return exp(-ll / num_samples);
}
ContextTreeAnalyzer UnigramRescalingSampler::GetCTAnalyzer() {
  return cmanager_.GetCTAnalyzer(dmanager_.intern());
}

void UnigramRescalingSampler::SampleLdaPart(int iteration_i) {
  double ll =0;
  for (size_t j = 0; j < sampling_idxs_.size(); ++j) {
    if (j % 1000 == 0) {
      cerr << "[" << setw(2) << (iteration_i + 1) << "] sampling ...\t" << setw(6)
           << (j + 1) << "/" << sampling_idxs_.size() << "\r";
    }
    auto& word = dmanager_.word(j);
    int type = dmanager_.token(*word);
    int topic = dmanager_.topic(*word);
    if (iteration_i > 0) {
      dmanager_.DecrementTopicCount(word->doc_id, topic);
      --topic2word_counts_[topic].first;      
      if (--topic2word_counts_[topic].second[type] == 0) {
        topic2word_counts_[topic].second.erase(type);
      }
    }
    topic_sampler_.InitWithTopicPrior(dmanager_.doc2topic_count()[word->doc_id]);
    CalcTopic2WordProb(type);
    topic_sampler_.TakeInLikelihood(topic2word_prob_);
    auto sample = topic_sampler_.Sample();
    dmanager_.IncrementTopicCount(word->doc_id, sample.topic, parameters_.topic_parameter().alpha[sample.topic]);
    dmanager_.set_topic(*word, sample.topic);
    ++topic2word_counts_[sample.topic].second[type];
    ++topic2word_counts_[sample.topic].first;
    ll += std::log(sample.p_w);
  }
  parameters_.SamplingAlpha(dmanager_.doc2topic_count(),
                            dmanager_.doc2topic2tables());
  double ppl = exp(-ll / sampling_idxs_.size());
  cerr << "[" << setw(2) << (iteration_i + 1) << "] sampling ...\t" << setw(6)
       << sampling_idxs_.size() << "/" << sampling_idxs_.size() << "\tPPL=" << ppl << "\r";
  LOG("info") << "[" << setw(2) << (iteration_i + 1) << "] PPL=" << ppl << endl;
}

void UnigramRescalingSampler::SampleHpyPart(int iteration_i) {
  for (size_t j = 0; j < sampling_idxs_.size(); ++j) {
    if (j % 1000 == 0) {
      cerr << "[" << setw(2) << (iteration_i + 1) << "] sampling ...\t" << setw(6)
           << (j + 1) << "/" << sampling_idxs_.size() << "\r";
    }
    auto& word = dmanager_.word(j);
    auto& sent = dmanager_.sentence(*word);
    int type = dmanager_.token(*word);
    if (iteration_i > 0) {
      cmanager_.UpTreeFromLeaf(word->node, word->depth);
      cmanager_.rmanager().RemoveCustomerFromPath(type, 0, word->depth);
    } else {
      word->depth = cmanager_.WalkTree(sent, word->token_idx - 1, 0, parameters_.ngram_order() - 1);
      word->node = cmanager_.current_node_path()[word->depth];
    }
    cmanager_.rmanager().CalcGlobalPredictivePath(type, word->depth);
    cmanager_.rmanager().AddCustomerToPath(type, 0, word->depth);
  }
  auto& depth2nodes = cmanager_.GetDepth2Nodes();
  parameters_.SamplingHpyParameter(depth2nodes);
  UpdateBeta();

  LOG("hyper") << "iteration: " << iteration_i << "\n"
               << parameters_.OutputHypers();
  LOG("hyper") << "beta: " << beta_ << endl;
}

void UnigramRescalingSampler::CalcTopic2WordProb(int type) {
  for (size_t i = 0; i < topic2word_prob_.size(); ++i) {
    double c = beta_;
    auto it = topic2word_counts_[i].second.find(type);
    if (it != topic2word_counts_[i].second.end()) {
      c += (*it).second;
    }
    topic2word_prob_[i] = c / (topic2word_counts_[i].first + beta_sum_);
  }
}
double UnigramRescalingSampler::UnigramWordProb(int type) const {
  return (double)unigram_counts_[type] / (double)unigram_sum_count_;
}
double UnigramRescalingSampler::HpyUnigramWordProb(int type) {
  cmanager_.rmanager().CalcGlobalPredictivePath(type, 0);
  return cmanager_.rmanager().depth2topic_predictives()[0][0];
}
void UnigramRescalingSampler::UpdateBeta() {
  double a = 1.0, b = 1.0;
  double s_d = 0;
  double z_kj_inv = 0;
  double log_w = 0;

  for (auto& word_count : topic2word_counts_) {
    s_d += random->NextBernoille(
        (word_count.first / beta_sum_) / (word_count.first / beta_sum_ + 1));
    log_w  += log(random->NextBeta(beta_sum_ + 1, word_count.first));
    for (auto& word : word_count.second) {
      int n_dk = word.second;
      if (n_dk >= 1) {
        z_kj_inv += 1;
        for (int j = 1; j < n_dk; ++j) {
          z_kj_inv += 1 - random->NextBernoille(j / (j + beta_));
        }
      }
    }
  }
  beta_sum_ = random->NextGamma(a - s_d + z_kj_inv, b - log_w);
  beta_ = beta_sum_ / unigram_counts_.size();
}

};
