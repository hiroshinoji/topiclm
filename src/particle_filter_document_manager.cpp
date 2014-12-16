#include <string>
#include "particle_filter_document_manager.hpp"
#include "util.hpp"
#include "io_util.hpp"

using namespace std;

namespace topiclm {

void ParticleFilterDocumentManager::Read(std::shared_ptr<Reader> reader) {
  int eos_id = intern_.key2id_nogen(kEosKey);

  doc2token_seq_ = reader->ReadDocuments(intern_);

  int num_tokens = 0;
  for (auto& doc : doc2token_seq_) {
    for (auto& sent : doc) {
      for (int type : sent) {
        if (type != eos_id) ++num_tokens;
      }
    }
  }
  // for (size_t i = 0; i < particle2topic_seq_.size(); ++i) {
  //   particle2topic_count_[i].first = 0;
  //   particle2topic_count_[i].second.resize(num_topics_ + 1);
  // }
  cerr << "lexicon: " << intern_.size() << endl;
  cerr << "tokens: " << num_tokens << endl;
  cerr << "documents: " << doc2token_seq_.size() << endl;
}

void ParticleFilterDocumentManager::SetCurrentDoc(int current_doc_id) {
  current_doc_id_ = current_doc_id;
  for (size_t i = 0; i < particle2topic_seq_.size(); ++i) {
    particle2topic_seq_[i] = doc2token_seq_[current_doc_id];
    for (auto& sent : particle2topic_seq_[i]) {
      for (size_t j = 0; j < sent.size(); ++j) sent[j] = -1;
    }
    particle2topic_count_[i].first = 0;
    particle2topic_count_[i].second = std::vector<int>(num_topics_ + 1, 0);
  }
  
  int eos_id = intern_.key2id(kEosKey);
  auto& current_doc = doc2token_seq_[current_doc_id];

  particle2words_.clear();
  particle2words_.resize(num_particles_);

  for (size_t i = 0; i < current_doc.size(); ++i) {
    for (size_t j = 1; j < current_doc[i].size() - 1; ++j) {
      assert(current_doc[i][j] != eos_id);
      for (int k = 0; k < num_particles_; ++k) {
        particle2words_[k].emplace_back(0, current_doc_id, i, j);
      }
    }
  }
}

void ParticleFilterDocumentManager::StoreSentence(
    const vector<int>& sentence,
    const vector<vector<int> >& particle2topics,
    const vector<int>& depths,
    bool consider_global) {

  auto length = sentence.size();
  
  assert(current_doc_id_ == 0); // this method should only be used on interactive mode
  
  auto sentence_idx = doc2token_seq_[0].size();
  
  doc2token_seq_[0].emplace_back(sentence.begin(), sentence.end());
  for (size_t particle = 0; particle < particle2topics.size(); ++particle) {
    auto& topics = particle2topics[particle];
    particle2topic_seq_[particle].emplace_back(topics.begin(), topics.begin() + length);
  }
  for (size_t particle = 0; particle < particle2words_.size(); ++particle) {
    for (size_t idx = 0; idx < length; ++idx) {
      int topic = particle2topics[particle][idx];
      
      particle2words_[particle].emplace_back(depths[idx], 0, sentence_idx, idx);
      auto& word = particle2words_[particle][particle2words_[particle].size() - 1];
      word.is_general = consider_global && particle2topics[particle][idx] == 0;

      IncrementTopicCount(particle, topic, word.is_general);
    }
  }
}

void ParticleFilterDocumentManager::Reset() {
  current_doc_id_ = 0;
  
  doc2token_seq_.clear();
  doc2token_seq_.resize(1);

  particle2words_.clear();
  particle2words_.resize(num_particles_);
  
  for (size_t particle = 0; particle < particle2topic_seq_.size(); ++particle) {
    particle2topic_seq_[particle].clear();
    
    particle2topic_count_[particle].first = 0;
    particle2topic_count_[particle].second = vector<int>(num_topics_ + 1, 0);
  }
}

}
