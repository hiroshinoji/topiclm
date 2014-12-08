#ifndef _HPY_LDA_ADD_REMOVE_RESULT_H_
#define _HPY_LDA_ADD_REMOVE_RESULT_H_

namespace hpy_lda {

enum AddRemoveResult {
  GlobalTableChanged = 0,
  LocalTableChanged,
  TableUnchanged,
};
  
};

#endif /* _HPY_LDA_ADD_REMOVE_RESULT_H_ */
