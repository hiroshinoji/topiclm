#ifndef _HPY_LDA_DIRICHLET_SAMPLER_HPP_
#define _HPY_LDA_DIRICHLET_SAMPLER_HPP_

#include <vector>

namespace hpy_lda {

struct DirichletParameter;

class DirichletSamplerInterface {
 public:
  virtual ~DirichletSamplerInterface() {}
  virtual void Update(
      const std::vector<std::pair<int, std::vector<int> > >& doc2topic_counts,
      DirichletParameter& topic_parameter) = 0;
  virtual void UpdateBeta(
      const std::vector<std::vector<std::vector<int> > >& doc2topic2tables,
      DirichletParameter& topic_parameter) = 0;
};

class UniformDirichletSampler : public DirichletSamplerInterface {
 public:
  virtual ~UniformDirichletSampler() {}  
  virtual void Update(
      const std::vector<std::pair<int, std::vector<int> > >& doc2topic_counts,
      DirichletParameter& topic_parameter);
  virtual void UpdateBeta(
      const std::vector<std::vector<std::vector<int> > >&, DirichletParameter&) {}
  
};

class NonUniformDirichletSampler : public UniformDirichletSampler {
 public:
  void UpdateBeta(
      const std::vector<std::vector<std::vector<int> > >& doc2topic2tables,
      DirichletParameter& topic_parameter);
  
};

// class GlobalSpecializedDirichletSampler : public DirichletSamplerInterface {
//  public:
//   void Update(
//       const std::vector<std::pair<int, std::vector<int> > >& doc2topic_counts,
//       DirichletParameter& topic_parameter);
// };

} // hpy_lda

#endif /* _HPY_LDA_DIRICHLET_SAMPLER_HPP_ */
