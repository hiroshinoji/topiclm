#ifndef _HPY_LDADOCUMENT_MANAGER_HPP_
#define _HPY_LDADOCUMENT_MANAGER_HPP_

#include <vector>
#include <string>
#include <pficommon/data/intern.h>
#include "word.hpp"
#include "random_util.hpp"
#include "util.hpp"

namespace hpy_lda {

class DirichletParameter;

class DocumentManager {
 public:
  DocumentManager() : num_topics_(0), ngram_order_(0) {}
  DocumentManager(DocumentManager&& other) 
    : words_{std::move(other.words_)},
      doc2token_seq_{std::move(other.doc2token_seq_)},
      doc2topic_seq_{std::move(other.doc2topic_seq_)},
      doc2topic_count_{std::move(other.doc2topic_count_)},
      intern_{std::move(other.intern_)},
      num_topics_{other.num_topics_},
      ngram_order_{ngram_order_}
  {}
  DocumentManager(int num_topics, int ngram_order)
      : num_topics_(num_topics), ngram_order_(ngram_order) {}
  ~DocumentManager() {}
  
  void Read(const std::string& fn);
  void OutputTopicAssign(std::ostream& os) const;
  void OutputTopicCount(std::ostream& os) const;
  
  void IncrementTopicCount(int doc_id,
                           int topic,
                           double alpha_k,
                           bool is_general = false) {
    if (!is_general) {
      ++doc2topic_count_[doc_id].first;
      if (alpha_k != 0) {
        AddCustomer(doc_id, topic, alpha_k);
      }
    }
    ++doc2topic_count_[doc_id].second[topic];
  }
  void DecrementTopicCount(int doc_id,
                           int topic,
                           bool is_general = false) {
    if (!is_general) {
      assert(--doc2topic_count_[doc_id].first >= 0);
      RemoveCustomer(doc_id, topic);
    }
    assert(--doc2topic_count_[doc_id].second[topic] >= 0);
  }
  void AddCustomer(int doc_id, int topic, double alpha_k) {
    if (doc2topic_count_[doc_id].first == 1) {
      assert(doc2topic2tables_[doc_id][topic].empty());
      doc2topic2tables_[doc_id][topic].push_back(1);
      return;
    }
    auto& tables = doc2topic2tables_[doc_id][topic];
    if (buffer_.size() <= tables.size() + 1) {
      buffer_.resize(tables.size() + 1);
    }
    for (size_t i = 0; i < tables.size(); ++i) buffer_[i] = tables[i];
    buffer_[tables.size()] = alpha_k;
    size_t sample = random->SampleUnnormalizedPdfRef(buffer_, tables.size());
    if (sample == tables.size()) {
      tables.push_back(1);
    } else {
      ++tables[sample];
    }
    
  }
  void RemoveCustomer(int doc_id, int topic) {
    if (doc2topic2tables_[doc_id][topic].empty()) return; // not hierarchical mode
    if (doc2topic_count_[doc_id].first == 0) {
      doc2topic2tables_[doc_id][topic].clear();
      return;
    }
    auto& tables = doc2topic2tables_[doc_id][topic];
    if (buffer_.size() <= tables.size()) {
      buffer_.resize(tables.size());
    }
    for (size_t i = 0; i < tables.size(); ++i) buffer_[i] = tables[i];
    auto sample = random->SampleUnnormalizedPdfRef(buffer_, tables.size() - 1);
    if (--tables[sample] == 0) {
      EraseAndShrink(tables, tables.begin() + sample);
    }
  }

  std::vector<int> GetUnigramCounts() const;

  int num_words() const { return words_.size(); }
  int num_docs() const { return doc2token_seq_.size(); }
  int lexicon() const { return intern_.size(); }
  pfi::data::intern<std::string>& intern() { return intern_; }
  
  const std::vector<std::pair<int, std::vector<int> > >& doc2topic_count() const {
    return doc2topic_count_;
  }
  const std::vector<std::vector<std::vector<int> > >& doc2topic2tables() const {
    return doc2topic2tables_;
  }
  const std::shared_ptr<Word> word(int word_idx) { return words_[word_idx]; }
  int token(const Word& word) const {
    return doc2token_seq_[word.doc_id][word.sent_idx][word.token_idx];
  }
  int topic(const Word& word) const {
    return doc2topic_seq_[word.doc_id][word.sent_idx][word.token_idx];
  }
  const std::vector<int>& sentence(const Word& word) const {
    return doc2token_seq_[word.doc_id][word.sent_idx];
  }

  void set_topic(Word& word, int topic) {
    doc2topic_seq_[word.doc_id][word.sent_idx][word.token_idx] = topic;
  }

 private:
  void BuildWords();
  
  std::vector<std::shared_ptr<Word> > words_;
  std::vector<std::vector<std::vector<int> > > doc2token_seq_;
  std::vector<std::vector<std::vector<int> > > doc2topic_seq_;
  
  std::vector<std::pair<int, std::vector<int> > > doc2topic_count_;
  std::vector<std::vector<std::vector<int> > > doc2topic2tables_;

  static std::vector<double> buffer_;

  pfi::data::intern<std::string> intern_;
  const int num_topics_;
  const int ngram_order_;

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(intern_);
    if (ar.is_read) {
      int num_topics, ngram_order;
      ar & num_topics & ngram_order;
      const_cast<int&>(num_topics_) = num_topics;
      const_cast<int&>(ngram_order_) = ngram_order;      
    } else {
      ar & const_cast<int&>(num_topics_);
      ar & const_cast<int&>(ngram_order_);
    }
  }
};

};

#endif /* _HPY_LDADOCUMENT_MANAGER_HPP_ */
