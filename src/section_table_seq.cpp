#include "section_table_seq.hpp"
#include "restaurant.hpp"

namespace topiclm {

void SectionTableSeq::Reset(Restaurant& r, int w, bool consider_zero) {
  auto& internal = r.internal(w);
  size_ = internal.GetTableSeqFromSection(table_seq_, consider_zero);
  ResetIdxs();
}

void SectionTableSeq::ResetIdxs() {
  if (idxs_.size() <= size_) {
    idxs_.resize(size_);
  }
  for (size_t i = 0; i < size_; ++i) {
    idxs_[i] = i;
  }
  std::random_shuffle(idxs_.begin(), idxs_.begin() + size_, *random);
}

} // topiclm
