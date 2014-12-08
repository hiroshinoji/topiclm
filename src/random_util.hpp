#ifndef _HPY_LDA_RANDOM_UTIL_HPP_
#define _HPY_LDa_RANDOM_UTIL_HPP_

#include "random.hpp"
#include <memory>

namespace hpy_lda {

extern std::unique_ptr<RandomBase> random;
extern std::unique_ptr<TemplatureManager> temp_manager;

void init_rnd(int seed);
void init_rnd();

} // namespace hpy_lda

#endif /* _HPY_LDA_RANDOM_UTIL_HPP_ */
