#include <iomanip>
#include <iostream>
#include "util.hpp"
#include "random_util.hpp"
#include "hpy_lda.hpp"
#include "document_manager.hpp"
#include "parameters.hpp"
#include "topic_sampler.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"
#include "lambda_manager.hpp"
#include "restaurant_manager.hpp"
#include "table_based_sampler.hpp"

using namespace std;

namespace hpy_lda {

unique_ptr<TopicDepthSampler> GetTopicDepthSampler(TreeType tree_type,
                                                   const Parameters& parameters) {
  auto p = tree_type == kGraphical ?
      unique_ptr<TopicDepthSampler>(new TopicDepthSampler(parameters)) :
      unique_ptr<TopicDepthSampler>(new NonGraphicalTopicDepthSampler(parameters));
  return p;
}

HpyLdaSampler::HpyLdaSampler(LambdaType lambda_type, TreeType tree_type, DocumentManager& dmanager, Parameters& parameters)
    : dmanager_(dmanager),
      parameters_(parameters),
      cmanager_(lambda_type, tree_type, parameters, dmanager.lexicon()),
      topic_sampler_(GetTopicDepthSampler(tree_type, parameters)),
      lambda_type_(lambda_type),
      tree_type_(tree_type) {
  int num_words = dmanager_.num_words();
  sampling_idxs_.clear();
  for (int i = 0; i < num_words; ++i) {
    sampling_idxs_.push_back(i);
  }
}

HpyLdaSampler::~HpyLdaSampler() {}

void HpyLdaSampler::InitializeInRandom(int init_depth, bool hpy_random, double p_global) {
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

    auto& node_path = cmanager_.current_node_path();
    for (int d = 0; d < word->depth; ++d) {
      node_path[d]->add_type2child(type, node_path[d+1]);
    }
    word->node = cmanager_.current_node_path()[word->depth];
    
    int sampled_topic = topic_sampler_->SampleAtRandom(p_global);
    if (ConsiderGeneral()) {
      word->is_general = sampled_topic == 0;
    }
    dmanager_.IncrementTopicCount(word->doc_id,
                                  sampled_topic,
                                  parameters_.topic_parameter().alpha[sampled_topic],
                                  word->is_general);
    dmanager_.set_topic(*word, sampled_topic);

    if (hpy_random) {
      cmanager_.rmanager().AddCustomerToPathAtRandom(type, sampled_topic, word->depth);
    } else {
      cmanager_.rmanager().CalcDepth2TopicPredictives(type, word->depth);
      cmanager_.rmanager().AddObservedCustomerToPath(type, sampled_topic, word->depth);
    }
    cmanager_.rmanager().CombineSectionToWord(type, sampled_topic, word->depth, word);
  }
  LOG("lambda") << "iteration: 0 \n"
                << cmanager_.PrintDepth2Tables() << endl;
}

double HpyLdaSampler::RunOneIteration(int iteration_i, bool table_sample) {
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
    int topic = dmanager_.topic(*word);
    int current_max_depth = word->depth;
    if (iteration_i > 0) {
      cmanager_.UpTreeFromLeaf(word->node, word->depth);
      cmanager_.rmanager().RemoveStopPassedCustomers(word->depth);
      current_max_depth
          = cmanager_.WalkTreeNoCreate(sent, word->token_idx - 1 - word->depth, word->depth);
      cmanager_.rmanager().SeparateWordFromSection(type, topic, word->depth, word);
      cmanager_.rmanager().RemoveObservedCustomerFromPath(type, topic, word->depth);
      dmanager_.DecrementTopicCount(word->doc_id, topic, word->is_general);
    } else {
      current_max_depth = cmanager_.WalkTreeNoCreate(sent, word->token_idx - 1, 0);
    }
    cmanager_.CalcStopPriorPath(current_max_depth, word->token_idx);
    // if (j == 1000) {
    //   double p_sum = 0;
    //   for (int i = 0; i < dmanager_.lexicon(); ++i) {
    //     cmanager_.rmanager().CalcDepth2TopicPredictives(i, current_max_depth);
    //     topic_sampler_->InitWithTopicPrior(dmanager_.doc2topic_count()[word->doc_id],
    //                                        cmanager_.lambda_path());
    //     topic_sampler_->TakeInStopPrior(cmanager_.stop_prior_path());
    //     topic_sampler_->TakeInLikelihood(cmanager_.rmanager().depth2topic_predictives());
    //     p_sum += topic_sampler_->CalcMarginal();
    //   }
    //   cerr << "\n p_sum = " << p_sum << endl;
    // }
    cmanager_.rmanager().CalcDepth2TopicPredictives(type, current_max_depth);

    topic_sampler_->InitWithTopicPrior(dmanager_.doc2topic_count()[word->doc_id],
                                       cmanager_.lambda_path());
    topic_sampler_->TakeInStopPrior(cmanager_.stop_prior_path());
    topic_sampler_->TakeInLikelihood(cmanager_.rmanager().depth2topic_predictives());
    auto sample = topic_sampler_->Sample();

    ll += std::log(sample.p_w);

    if (ConsiderGeneral()) {
      word->is_general = sample.topic == 0;
    }

    dmanager_.IncrementTopicCount(word->doc_id,
                                  sample.topic,
                                  parameters_.topic_parameter().alpha[sample.topic],
                                  word->is_general);
    dmanager_.set_topic(*word, sample.topic);

    if (sample.depth > current_max_depth) { // sample deep node
      cmanager_.WalkTree(sent, word->token_idx - 1 - current_max_depth, current_max_depth, sample.depth);
    } else if (sample.depth < current_max_depth) {
      cmanager_.EraseEmptyNodes(current_max_depth, sample.depth);
    }
    word->depth = sample.depth;

    // TODO separate this logic to other methods
    if (word->node != cmanager_.current_node_path()[sample.depth]) {
      auto& node_path = cmanager_.current_node_path();
      for (int d = 0; d < sample.depth; ++d) {
        node_path[d]->add_type2child(type, node_path[d+1]);
      }
    }
    word->node = cmanager_.current_node_path()[sample.depth];
    
    cmanager_.rmanager().CombineSectionToWord(type, sample.topic, word->depth, word);
    
    cmanager_.rmanager().AddStopPassedCustomers(word->depth);
    cmanager_.rmanager().AddObservedCustomerToPath(type, sample.topic, word->depth);
  }
  auto& depth2nodes = cmanager_.GetDepth2Nodes();
  parameters_.SamplingHpyParameter(depth2nodes);
  parameters_.SamplingAlpha(dmanager_.doc2topic_count(),
                            dmanager_.doc2topic2tables());
  if (iteration_i > 5) {
    parameters_.SamplingLambdaConcentration(depth2nodes);
  }
  if (iteration_i % 10 == 0) {
    LOG("hyper") << "iteration: " << iteration_i << "\n"
                 << parameters_.OutputHypers() << endl;
    LOG("lambda") << "iteration: " << iteration_i << "\n"
                  << cmanager_.PrintDepth2Tables() << endl;
    cmanager_.tsampler().ResetCacheInFloorSampler();
  }
  if (table_sample) {
    cmanager_.TableBasedResample();
  }
  
  double ppl = std::exp(-ll / sampling_idxs_.size());
  cerr << "[" << setw(2) << (iteration_i + 1) << "] sampling ...\t" << setw(6)
       << sampling_idxs_.size() << "/" << sampling_idxs_.size()
       << "\tPPL=" << ppl << "\r";
      //<< "\ttopicLL=" << logjoint() << "\r";
  ll = logjoint();
  LOG("info") << "[" << setw(2) << (iteration_i + 1)
              << "] PPL=" << ppl
              << " LL=" << ll << endl;
  return ll;
}

ParticleFilterSampler
HpyLdaSampler::GetParticleFilterSampler(ParticleFilterDocumentManager& pf_dmanager) {
  return ParticleFilterSampler(*this, pf_dmanager);
}
ContextTreeAnalyzer HpyLdaSampler::GetCTAnalyzer() {
  return cmanager_.GetCTAnalyzer(dmanager_.intern());
}
void HpyLdaSampler::set_table_based_sampler(int max_t_in_block, int max_c_in_block, bool include_root) {
  cmanager_.set_table_based_sampler(tree_type_, dmanager_);
  cmanager_.tsampler().set_max_t_in_block(max_t_in_block);
  cmanager_.tsampler().set_max_c_in_block(max_c_in_block);
  cmanager_.tsampler().set_include_root(include_root);
}

double HpyLdaSampler::logjoint() const {
  double ll = 0;
  double alpha_1 = parameters_.topic_parameter().alpha_1;
  auto& doc2topic_counts = dmanager_.doc2topic_count();
  for (auto& doc2topic_count : doc2topic_counts) {
    auto sum_count = doc2topic_count.first;
    ll += lgamma(alpha_1) - lgamma(alpha_1 + sum_count);
    for (size_t k = 1; k < doc2topic_count.second.size(); ++k) {
      double alpha = parameters_.topic_parameter().alpha[k];
      ll += lgamma(doc2topic_count.second[k] + alpha) - lgamma(alpha);
    }
  }

  // CRP for each topic part
  auto& depth2nodes = cmanager_.GetDepth2Nodes();
  for (size_t i = 0; i < depth2nodes.size(); ++i) {
    for (auto node : depth2nodes[i]) {
      auto& r = node->restaurant();
      //r.CheckConsistency();
      ll += r.logjoint(parameters_.hpy_parameter(), i);
    }
  }
  auto& r = (*depth2nodes[0].begin())->restaurant();
  //int zero_tables = r.floor_sum_tables(0);
  int tables_from_base_measure = 0;
  for (auto& internal : r.type2internal()) {
    for (int i = 0; i < parameters_.topic_parameter().num_topics + 1; ++i) {
      tables_from_base_measure += internal.second.labeled_tables(i, -1);
    }
  }
  ll += log(1.0 / double(dmanager_.lexicon())) * tables_from_base_measure;

  // CRP of lambda part:
  // if top level lambda = 1 (when global topic is not considered), this part is diminished to 0
  double root_lambda = cmanager_.lmanager().root_lambda();
  if (root_lambda > 0 && root_lambda < 1) {
    ll += log(root_lambda) * r.local_labeled_table_tables();
    ll += log(1-root_lambda) * r.global_labeled_tables();
    for (size_t i = 0; i < depth2nodes.size(); ++i) {
      for (auto node : depth2nodes[i]) {
        auto& r = node->restaurant();
        ll += r.logjoint_lambda(parameters_.lambda_parameter().c[i]);
      }
    }
  }

  return ll;
}

} // namespace hpy_lda;
