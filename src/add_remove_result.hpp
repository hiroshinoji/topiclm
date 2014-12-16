#ifndef _TOPICLM_ADD_REMOVE_RESULT_H_
#define _TOPICLM_ADD_REMOVE_RESULT_H_

namespace topiclm {

enum AddRemoveResult {
  GlobalTableChanged = 0,
  LocalTableChanged,
  TableUnchanged,
};
  
};

#endif /* _TOPICLM_ADD_REMOVE_RESULT_H_ */
