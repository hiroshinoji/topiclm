#include "random_util.hpp"

namespace hpy_lda {

std::unique_ptr<RandomBase> random;
std::unique_ptr<TemplatureManager> temp_manager;

void init_rnd(int seed) {
  if (temp_manager == nullptr) {
    temp_manager.reset(new TemplatureManager());
  }
  random.reset(new RandomMT(seed, *temp_manager));
}
void init_rnd() {
  if (temp_manager == nullptr) {
    temp_manager.reset(new TemplatureManager());
  }  
  std::random_device rd;
  random.reset(new RandomMT(rd(), *temp_manager));
}
  
} // namespace hpy_lda

