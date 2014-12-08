#include <string>
#include "document_manager.hpp"
#include "util.hpp"

using namespace std;

namespace hpy_lda {

vector<double> DocumentManager::buffer_;

void DocumentManager::Read(const string& fn) {
  string unk_type = "__unk__";
  intern_.clear();
  intern_.key2id("EOS");
  intern_.key2id(unk_type);
  doc2token_seq_ = ReadDocs(fn, "__unk__", intern_, true);
  doc2topic_seq_ = doc2token_seq_;
  for (auto& doc : doc2topic_seq_) {
    for (auto& sent : doc) {
      for (size_t i = 0; i < sent.size(); ++i) sent[i] = -1;
    }
  }
  doc2topic_count_.resize(doc2token_seq_.size());
  doc2topic2tables_.resize(doc2token_seq_.size());
  for (size_t i = 0; i < doc2topic_count_.size(); ++i) {
    doc2topic_count_[i].first = 0;
    doc2topic_count_[i].second.assign(num_topics_ + 1, 0);
    typedef decltype(doc2topic2tables_) tables_t;
    doc2topic2tables_[i].assign(num_topics_ + 1, tables_t::value_type::value_type{});
  }
  BuildWords();
  cerr << "lexicon: " << intern_.size() << endl;
  cerr << "tokens: " << words_.size() << endl;
  cerr << "documents: " << doc2token_seq_.size() << endl;
}
void DocumentManager::OutputTopicAssign(ostream& os) const {
  for (auto& doc : doc2topic_seq_) {
    for (auto& sent : doc) {
      for (size_t i = 0; i < sent.size(); ++i) {
        if (sent[i] == -1) {
          os << "- ";
        } else {
          os << sent[i] << " ";
        }
      }
      os << endl;
    }
    os << endl;
  }
}
void DocumentManager::OutputTopicCount(ostream& os) const {
  for (size_t i = 0; i < doc2topic_count_.size(); ++i) {
    os << "[" << i << "] all:" << doc2topic_count_[i].first << " ";
    for (size_t j = 0; j < doc2topic_count_[i].second.size(); ++j) {
      os << j << ":" << doc2topic_count_[i].second[j] << " ";
    }
    os << endl;
  }
}

vector<int> DocumentManager::GetUnigramCounts() const {
  vector<int> unigram_counts(lexicon());
  for (auto& word : words_) {
    ++unigram_counts[token(*word)];
  }
  return unigram_counts;
}

void DocumentManager::BuildWords() {
  words_.clear();
  for (size_t i = 0; i < doc2token_seq_.size(); ++i) {
    auto& observed_seq = doc2token_seq_[i];
    for (size_t j = 0; j < observed_seq.size(); ++j) {
      for (size_t k = 1; k < observed_seq[j].size(); ++k) {
        words_.push_back(std::make_shared<Word>(0, i, j, k));
      }
    }
  }
}

} // hpy_lda
