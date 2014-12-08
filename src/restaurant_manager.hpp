#ifndef _HPY_LDA_RESTAURANT_MANAGER_HPP_
#define _HPY_LDA_RESTAURANT_MANAGER_HPP_

#include <vector>
#include "word.hpp"
#include "add_remove_result.hpp"
#include "config.hpp"

namespace hpy_lda {

class Node;
class LambdaManagerInterface;
class Parameters;
class HPYParameter;

class RestaurantManager {
 public:
  RestaurantManager(const std::vector<Node*>& node_path,
                    LambdaManagerInterface& lmanager,
                    const Parameters& parameters,
                    double zero_order_pred);
  virtual ~RestaurantManager();
  virtual void AddCustomerToPath(int type, topic_t topic, int word_depth);
  virtual void AddObservedCustomerToPath(int type, topic_t topic, int word_depth) {
    AddCustomerToPath(type, topic, word_depth);
  }
  void AddCustomerToCache(int type, topic_t topic, int word_depth, topic_t doc_id);
  std::pair<AddRemoveResult, topic_t> RemoveCustomerFromCache(int type, int word_depth, topic_t doc_id);
  
  virtual void AddCustomerToPathAtRandom(int type, topic_t topic, int word_depth);
  virtual void RemoveCustomerFromPath(int type, topic_t topic, int word_depth);
  virtual void RemoveObservedCustomerFromPath(int type, topic_t topic, int word_depth) {
    RemoveCustomerFromPath(type, topic, word_depth);
  }
  void AddStopPassedCustomers(int word_depth) const;
  void RemoveStopPassedCustomers(int word_depth) const;

  void CombineSectionToWord(int type,
                            topic_t floor_id,
                            int word_depth,
                            const std::shared_ptr<Word>& word_ptr,
                            bool cache = false);
  void SeparateWordFromSection(int type,
                               topic_t floor_id,
                               int word_depth,
                               const std::shared_ptr<Word>& word_ptr,
                               bool cache = false);

  void CalcGlobalPredictivePath(int type, int word_depth);
  virtual void CalcLocalPredictivePath(int type, topic_t topic, int word_depth);
  virtual void CalcDepth2TopicPredictives(int type, int word_depth, bool test = false);
  void CalcMixtureParentPredictive(int word_depth);
  void CalcCachePath(int type, int word_depth, topic_t cache_id);

  double predictive(int depth, topic_t topic) const {
    if (depth < 0) return zero_order_predictives_[topic];
    return depth2topic_predictives_[depth][topic];
  }
  const std::vector<double> depth_predictives(int depth) const {
    if (depth < 0) return zero_order_predictives_;
    return depth2topic_predictives_[depth];
  }
  const std::vector<std::vector<double> >& depth2topic_predictives() const {
    return depth2topic_predictives_;
  }
  const std::vector<std::pair<double, double> >& cache_path() const {
    return cache_path_;
  }
  
 protected:
  const std::vector<Node*>& node_path_;
  LambdaManagerInterface& lmanager_;
  const HPYParameter& hpy_parameter_;

  std::vector<std::vector<double> > depth2topic_predictives_;
  std::vector<std::pair<double, double> > cache_path_;
  const std::vector<double> zero_order_predictives_;
};

class NonGraphicalRestaurantManager : public RestaurantManager {
 public:
  NonGraphicalRestaurantManager(const std::vector<Node*>& node_path,
                                LambdaManagerInterface& lmanager,
                                const Parameters& parameters,
                                double zero_order_pred);
  virtual ~NonGraphicalRestaurantManager();
  
  virtual void AddCustomerToPath(int type, topic_t topic, int word_depth);
  virtual void AddObservedCustomerToPath(int type, topic_t topic, int word_depth);
  virtual void AddCustomerToPathAtRandom(int type, topic_t topic, int word_depth);
  virtual void RemoveCustomerFromPath(int type, topic_t topic, int word_depth);
  virtual void RemoveObservedCustomerFromPath(int type, topic_t topic, int word_depth);
  
  virtual void CalcLocalPredictivePath(int type, topic_t topic, int word_depth);
  virtual void CalcDepth2TopicPredictives(int type, int word_depth, bool test = false);

};

} // hpy_lda


#endif /* _HPY_LDA_RESTAURANT_MANAGER_HPP_ */
