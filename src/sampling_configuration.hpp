#ifndef _TOPICLM_SAMPLING_CONFIGURATION_HPP_
#define _TOPICLM_SAMPLING_CONFIGURATION_HPP_

namespace topiclm {

struct SamplingConfiguration {
  enum UpdateMethod { kSlice = 0,
                      kAuxiliary,
                      kFixedIteration,
                      kNoUpdate,
                      kMetropolis,
                      KMetropolisAssymmetry,
                      kGlobalAssymmetry,
                      kAuxiliaryForeach };
  UpdateMethod lambda_update_method;
  UpdateMethod alpha_update_method;
  UpdateMethod hpylm_update_method;

  int resampling_table_labels_step; // if 0, no update
  int reestimate_alpha_start;
  
  int iteration_num;
};

} // topiclm

#endif /* _TOPICLM_SAMPLING_CONFIGURATION_HPP_ */
