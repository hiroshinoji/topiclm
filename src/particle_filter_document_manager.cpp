#include <string>
#include "particle_filter_document_manager.hpp"
#include "util.hpp"

using namespace std;

namespace hpy_lda {

void ParticleFilterDocumentManager::Read(const string& fn) {
  string unk_type = "__unk__";
  int eos_id = intern_.key2id_nogen("EOS");
  assert(eos_id == 0);
  assert(intern_.key2id_nogen(unk_type) == 1);
  doc2token_seq_ = ReadDocs(fn, "__unk__", intern_, false);

  int num_tokens = 0;
  for (auto& doc : doc2token_seq_) {
    for (auto& sent : doc) {
      for (int type : sent) {
        if (type != eos_id) ++num_tokens;
      }
    }
  }
  for (size_t i = 0; i < particle2topic_seq_.size(); ++i) {
    particle2topic_count_[i].first = 0;
    particle2topic_count_[i].second.resize(num_topics_ + 1);
  }
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
  
  int eos_id = intern_.key2id("EOS");
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

}
