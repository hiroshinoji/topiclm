#ifndef _HPY_LDA_PARTICLE_FILTER_DOCUMENT_MANAGER_HPP_
#define _HPY_LDA_PARTICLE_FILTER_DOCUMENT_MANAGER_HPP_

#include <pficommon/data/intern.h>
#include "word.hpp"

namespace hpy_lda {

class ParticleFilterDocumentManager {
 public:
  ParticleFilterDocumentManager(pfi::data::intern<std::string>& intern,
                                int num_particles,
                                int num_topics,
                                int ngram_order)
      : particle2topic_seq_(num_particles),
        particle2topic_count_(num_particles),
        intern_(intern),
        num_particles_(num_particles),
        num_topics_(num_topics),
        ngram_order_(ngram_order) {}
  ~ParticleFilterDocumentManager() {}
  
  void Read(const std::string& fn);
  void SetCurrentDoc(int current_doc_id);
  void IncrementTopicCount(int particle, int topic, bool is_general = false) {
    if (!is_general) {
      ++particle2topic_count_[particle].first;
    }
    ++particle2topic_count_[particle].second[topic];
  }
  void DecrementTopicCount(int particle, int topic, bool is_general = false) {
    if (!is_general) {
      --particle2topic_count_[particle].first;
    }
    --particle2topic_count_[particle].second[topic];
  }

  int doc_num_words() { return particle2words_[0].size(); }
  int num_docs() const { return doc2token_seq_.size(); }
  int num_particles() const { return num_particles_; }
  int lexicon() const { return intern_.size(); }
  const pfi::data::intern<std::string>& intern() { return intern_; }
  
  const std::pair<int, std::vector<int> >& topic_count(int particle) const {
    return particle2topic_count_[particle];
  }
  Word& word(int particle, int idx) {
    return particle2words_[particle][idx];
  }
  int token(const Word& word) const {
    return doc2token_seq_[word.doc_id][word.sent_idx][word.token_idx];
  }
  int topic(int particle, const Word& word) const {
    return particle2topic_seq_[particle][word.sent_idx][word.token_idx];
  }
  const std::vector<int>& sentence(const Word& word) const {
    return doc2token_seq_[word.doc_id][word.sent_idx];
  }

  void set_topic(int particle, Word& word, int topic) {
    particle2topic_seq_[particle][word.sent_idx][word.token_idx] = topic;
  }

 private:
  std::vector<std::vector<std::vector<int> > > doc2token_seq_;
  std::vector<std::vector<std::vector<int> > > particle2topic_seq_;
  std::vector<std::pair<int, std::vector<int> > > particle2topic_count_;

  std::vector<std::vector<Word> > particle2words_;

  int current_doc_id_;
  pfi::data::intern<std::string>& intern_;
  const int num_particles_;
  const int num_topics_;
  const int ngram_order_;
};

} // hpy_lda

#endif /* _HPY_LDA_PARTICLE_FILTER_DOCUMENT_MANAGER_HPP_ */
