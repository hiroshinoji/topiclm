#ifndef _TOPICLM_SECTION_TABLE_SEQ_HPP_
#define _TOPICLM_SECTION_TABLE_SEQ_HPP_

#include <memory>
#include "table_info.hpp"

namespace topiclm {

class Restaurant;

class SectionTableSeq {
 public:
  void Reset(Restaurant& r, int w, bool consider_zero);
  TableInfo& operator[](size_t idx) {
    return table_seq_[idxs_[idx]];
  }
  const TableInfo& operator[](size_t idx) const {
    return table_seq_[idxs_[idx]];
  }
  size_t size() const { return size_; }
  
 private:
  void ResetIdxs();
  
  std::vector<TableInfo> table_seq_;
  std::vector<size_t> idxs_;
  size_t size_;
};

} // topiclm


#endif /* _TOPICLM_SECTION_TABLE_SEQ_HPP_ */
