#ifndef _HPY_LDA_HPY_SAMPLER_HPP_
#define _HPY_LDA_HPY_SAMPLER_HPP_

#include <vector>
#include <memory>
#include <set>

namespace hpy_lda {

class Node;
class HPYParameter;

class HpySamplerInterface {
 public:
  virtual ~HpySamplerInterface() {}
  virtual void Update(
      const std::vector<std::set<Node*> >& depth2wnodes,
      HPYParameter& hpy_parameter) = 0;
};

class UniformHpySampler : public HpySamplerInterface {
 public:
  UniformHpySampler(int num_topics) : num_topics_(num_topics) {}
  void Update(
      const std::vector<std::set<Node*> >& depth2wnodes,
      HPYParameter& hpy_parameter);
 private:
  double SampleConcentration(
      std::vector<std::set<Node*> >::const_iterator depth_node_begin,
      std::vector<std::set<Node*> >::const_iterator depth_node_end,
      double concentration,
      double discount);
  double SampleDiscount(
      std::vector<std::set<Node*> >::const_iterator depth_node_begin,
      std::vector<std::set<Node*> >::const_iterator depth_node_end,
      double concentration,
      double discount);
  double SampleCacheConcentration(
      const std::vector<std::set<Node*> >& depth2wnodes,
      double concentration,
      double discount);
  double SampleCacheDiscount(
      const std::vector<std::set<Node*> >& some_depth_nodes,
      double concentration,
      double discount);
  
  int num_topics_;
};

class NonUniformHpySampler : public HpySamplerInterface {
 public:
  NonUniformHpySampler(int num_topics) : num_topics_(num_topics) {}
  void Update(
      const std::vector<std::set<Node*> >& depth2wnodes,
      HPYParameter& hpy_parameter);
 private:
  double SampleConcentration(
      std::vector<std::set<Node*> >::const_iterator depth_node_begin,
      std::vector<std::set<Node*> >::const_iterator depth_node_end,
      int topic,
      double concentration,
      double discount);
  double SampleDiscount(
      std::vector<std::set<Node*> >::const_iterator depth_node_begin,
      std::vector<std::set<Node*> >::const_iterator depth_node_end,
      int topic,
      double concentration,
      double discount);
  int num_topics_;
};

} // hpy_lda

#endif /* _HPY_LDA_HPY_SAMPLER_HPP_ */
