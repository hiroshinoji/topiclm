#ifndef _TOPICLM_RANDOM_UTIL_HPP_
#define _Topiclm_RANDOM_UTIL_HPP_

#include "random.hpp"
#include <memory>

namespace topiclm {

extern std::unique_ptr<RandomBase> random;
extern std::unique_ptr<TemplatureManager> temp_manager;

void init_rnd(int seed);
void init_rnd();

} // namespace topiclm

#endif /* _TOPICLM_RANDOM_UTIL_HPP_ */
