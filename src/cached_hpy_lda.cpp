#include <iomanip>
#include <iostream>
#include "util.hpp"
#include "random_util.hpp"
#include "cached_hpy_lda.hpp"
#include "document_manager.hpp"
#include "parameters.hpp"
#include "topic_sampler.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"
#include "lambda_manager.hpp"
#include "restaurant_manager.hpp"

using namespace std;

namespace hpy_lda {

CachedHpyLdaSampler::CachedHpyLdaSampler(LambdaType lambda_type, TreeType tree_type, DocumentManager& dmanager, Parameters& parameters)
  : dmanager_(dmanager),
    parameters_(parameters),
    cmanager_(lambda_type, tree_type, parameters, dmanager.lexicon()),
    topic_sampler_(new CachedTopicDepthSampler(parameters)),
    lambda_type_(lambda_type),
    tree_type_(tree_type) {
  int num_words = dmanager_.num_words();
  sampling_idxs_.clear();
  for (int i = 0; i < num_words; ++i) {
    sampling_idxs_.push_back(i);
  }
}

CachedHpyLdaSampler::~CachedHpyLdaSampler() {}

void CachedHpyLdaSampler::InitializeInRandom(int init_depth, bool hpy_random) {
  assert(sampling_idxs_.size() > 0);
  cerr << "initializting..." << endl;
  random_shuffle(sampling_idxs_.begin(), sampling_idxs_.end(), *random);
  for (size_t j = 0; j < sampling_idxs_.size(); ++j) {
    auto& word = dmanager_.word(j);
    auto& sent = dmanager_.sentence(*word);
    int type = dmanager_.token(*word);

    if (init_depth == 0) {
      word->depth = random->NextMult(parameters_.ngram_order());      
    } else {
      word->depth = init_depth;
    }

    word->depth = cmanager_.WalkTree(sent, word->token_idx - 1, 0, word->depth);
    cmanager_.rmanager().AddStopPassedCustomers(word->depth);
    word->node = cmanager_.current_node_path()[word->depth];
    
    int sampled_topic = topic_sampler_->SampleAtRandom();
    cmanager_.rmanager().AddCustomerToCache(type, sampled_topic, word->depth, word->doc_id);
    dmanager_.IncrementTopicCount(word->doc_id, sampled_topic, false);

    if (hpy_random) {
      cmanager_.rmanager().AddCustomerToPathAtRandom(type, sampled_topic, word->depth);      
    } else {
      cmanager_.rmanager().CalcDepth2TopicPredictives(type, word->depth);
      cmanager_.rmanager().AddCustomerToPath(type, sampled_topic, word->depth);
    }
  }
  LOG("lambda") << "iteration: 0 \n"
                << cmanager_.PrintDepth2Tables() << endl;
}

void CachedHpyLdaSampler::RunOneIteration(int iteration_i) {
  random_shuffle(sampling_idxs_.begin(), sampling_idxs_.end(), *random);
  double ll = 0;
  for (size_t j = 0; j < sampling_idxs_.size(); ++j) {
    if (j % 1000 == 0) {
      cerr << "[" << setw(2) << (iteration_i + 1) << "] sampling ...\t" << setw(6)
           << (j + 1) << "/" << sampling_idxs_.size() << "\r";
    }
    auto& word = dmanager_.word(j);
    auto& sent = dmanager_.sentence(*word);
    int type = dmanager_.token(*word);

    int current_max_depth = word->depth;
    if (iteration_i > 0) {
      cmanager_.UpTreeFromLeaf(word->node, word->depth);
      cmanager_.rmanager().RemoveStopPassedCustomers(word->depth);
      current_max_depth
          = cmanager_.WalkTreeNoCreate(sent, word->token_idx - 1 - word->depth, word->depth);
      auto remove_result = cmanager_.rmanager().RemoveCustomerFromCache(type, word->depth, word->doc_id);
      if (remove_result.first != TableUnchanged) {
        assert(remove_result.first == GlobalTableChanged);
        int old_topic = remove_result.second;
        cmanager_.rmanager().RemoveCustomerFromPath(type, old_topic, word->depth);
        dmanager_.DecrementTopicCount(word->doc_id, old_topic, word->is_general);
      }
    } else {
      current_max_depth = cmanager_.WalkTreeNoCreate(sent, word->token_idx - 1, 0);
    }
    cmanager_.CalcStopPriorPath(current_max_depth, word->token_idx);
    cmanager_.rmanager().CalcDepth2TopicPredictives(type, current_max_depth);
    cmanager_.rmanager().CalcCachePath(type, current_max_depth, word->doc_id);

    topic_sampler_->InitWithTopicPrior(dmanager_.doc2topic_count()[word->doc_id],
                                       cmanager_.lambda_path());
    topic_sampler_->TakeInStopPrior(cmanager_.stop_prior_path());
    topic_sampler_->TakeInLikelihood(cmanager_.rmanager().depth2topic_predictives());
    topic_sampler_->TakeInCache(cmanager_.cache_path());
    auto sample = topic_sampler_->Sample();
    
    ll += std::log(sample.p_w);
    
    if (sample.depth > current_max_depth) { // sample deep node
      cmanager_.WalkTree(sent, word->token_idx - 1 - current_max_depth, current_max_depth, sample.depth);
    } else if (sample.depth < current_max_depth) {
      cmanager_.EraseEmptyNodes(current_max_depth, sample.depth);
    }
    word->depth = sample.depth;
    word->node = cmanager_.current_node_path()[sample.depth];
    
    cmanager_.rmanager().AddStopPassedCustomers(word->depth);
    if (sample.cache) {
      assert(sample.topic == parameters_.topic_parameter().num_topics + 1);
      cmanager_.rmanager().AddCustomerToCache(type, -1, word->depth, word->doc_id);
    } else {
      cmanager_.rmanager().AddCustomerToCache(type, sample.topic, word->depth, word->doc_id);
      cmanager_.rmanager().AddCustomerToPath(type, sample.topic, word->depth);
      dmanager_.IncrementTopicCount(word->doc_id, sample.topic, false);
    }
  }
  auto& depth2nodes = cmanager_.GetDepth2Nodes();
  parameters_.SamplingHpyParameter(depth2nodes);
  parameters_.SamplingAlpha(dmanager_.doc2topic_count(),
                            dmanager_.doc2topic2tables());
  if (iteration_i > 50) {
    parameters_.SamplingLambdaConcentration(depth2nodes);
  }
  if (iteration_i % 5 == 0) {
    cmanager_.ResamplingTableLabels();
  }

  if (iteration_i < 10 || iteration_i % 10 == 0) {
    LOG("lambda") << "iteration: " << iteration_i << "\n"
                  << cmanager_.PrintDepth2Tables() << endl;
    LOG("hyper") << "iteration: " << iteration_i << "\n"
                 << parameters_.OutputHypers() << endl;
  }
  double ppl = std::exp(-ll / sampling_idxs_.size());
  cerr << "[" << setw(2) << (iteration_i + 1) << "] sampling ...\t" << setw(6)
       << sampling_idxs_.size() << "/" << sampling_idxs_.size() << "\tPPL=" << ppl << "\r";
  LOG("info") << "[" << setw(2) << (iteration_i + 1) << "] PPL=" << ppl << endl;
}

ContextTreeAnalyzer CachedHpyLdaSampler::GetCTAnalyzer() {
  return cmanager_.GetCTAnalyzer(dmanager_.intern());
}

} // hpy_lda
