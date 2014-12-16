#ifndef _TOPICLM_CONTEXT_TREE_ANALYZER_HPP_
#define _TOPICLM_CONTEXT_TREE_ANALYZER_HPP_

#include <vector>
#include <string>
#include <ostream>
#include <pficommon/data/intern.h>
#include "config.hpp"

namespace topiclm {

class ContextTreeManager;

class ContextTreeAnalyzer {
 public:
  ContextTreeAnalyzer(ContextTreeManager& ct_manager,
                      const pfi::data::intern<std::string>& intern);
  void AnalyzeRestaurant(const std::vector<std::string>& ngram, std::ostream& os) const;
  void AnalyzeTopic(const std::vector<std::string>& ngram, std::ostream& os) const;
  void AnalyzeLambdaToNgrams(std::ostream& os) const;
  void CalcTopicalNgrams(int K, std::ostream& os) const;
  void CalcRelativeTopicalNgrams(int K, std::ostream& os) const;
  int CountNodes() const;
  void CheckInternalConsistency() const;
  void LogAllNgrams() const;
  
 private:
  std::vector<int> ToIdNgram(const std::vector<std::string>& ngram) const {
    std::vector<int> id_ngram;
    for (auto& type : ngram) {
      int id = intern_.key2id_nogen(type);
      id_ngram.push_back(id);
    }
    return id_ngram;
  }

  ContextTreeManager& ct_manager_;
  const pfi::data::intern<std::string>& intern_;
  std::string unk_type_;  
};

} // topiclm

#endif /* _TOPICLM_CONTEXT_TREE_ANALYZER_HPP_ */
