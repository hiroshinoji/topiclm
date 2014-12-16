#ifndef _TOPICLM_RESTAURANT_HPP_
#define _TOPICLM_RESTAURANT_HPP_

#include <cassert>
#include <map>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <pficommon/text/json.h>
#include "serialization.hpp"
#include "add_remove_result.hpp"
#include "config.hpp"
#include "histogram_table_restaurant.hpp"
#include "internal_restaurant.hpp"
#include "lambda_manager.hpp"
#include "parameters.hpp"

namespace topiclm {

// defined below
double ComputePYPPredictive(int cw,
                            int tw,
                            int c,
                            int t,
                            double parent_probability,
                            double discount,
                            double concentration);

//class Restaurant : public PoolObject<Restaurant> {
class Restaurant {
 public:
  static void SetBufferSize(size_t s);
  Restaurant() : num_stop_customers_(0), num_pass_customers_(0) {}
  std::pair<AddRemoveResult, topic_t> AddCustomer(
      topic_t floor_id,
      int type,
      double local_parent_probability,
      const std::vector<double>& global_parent_pdf,
      double lambda,
      const std::vector<double>& label_priors,
      double discount,
      double concentration);
  AddRemoveResult AddCustomerNewTable(topic_t floor_id, int type, double lambda);
  std::pair<AddRemoveResult, topic_t> RemoveCustomer(topic_t floor_id, int type, bool cache = false);
  
  void ChangeTableLabelAtRandom(topic_t floor_id, int type, topic_t old_label, topic_t new_label);
  void ChangeTableLabel(topic_t floor_id, int type, int customers, topic_t old_label, topic_t new_label);
  void CombineSectionToWord(topic_t floor_id,
                            int type,
                            const std::shared_ptr<Word>& word_ptr,
                            bool cache = false);
  void SeparateWordFromSection(topic_t floor_id,
                               int type,
                               const std::shared_ptr<Word>& word_ptr,
                               bool cache = false);
  void AddCustomerToCache(topic_t cache_id, int type, topic_t topic, double discount);
  
  AddRemoveResult AddTable(bool is_global,
                           double parent_probability,
                           double c) {
    return table_restaurant_.AddCustomer(is_global, 0, parent_probability, 1.0, c);
  }
  AddRemoveResult AddTableNewTable(bool is_global, double lambda) {
    return table_restaurant_.AddCustomerNewTable(is_global, lambda);
  }
  AddRemoveResult RemoveTable(bool is_global) {
    return table_restaurant_.RemoveCustomer(is_global);
  }
  double AddTableNewTableFractionary(bool is_global,
                                     double enter_customer,
                                     double parent_probability,
                                     double c) {
    return table_restaurant_.AddCustomerFractionary(
        is_global, enter_customer, parent_probability, c);
  }
  void ResetFractionalCount() {
    table_restaurant_.ResetFractionalCount();
  }

  SampleTableInfo SampleTableAtRandom(topic_t floor_id, int type);
  
  void ChangeTablesFloor(topic_t floor_id, int type, topic_t label, const std::vector<int>& table_customers, topic_t new_floor_id);

  void ResetCache() {
    // boost::container::flat_map<topic_t, std::pair<int, int> >().swap(cache2c_t_);
    // for (auto& internal : type2internal_) {
    //   internal.second.ResetCache();
    // }
  }
  bool Empty() const {
    return floor2c_t_.empty();
  }
  void CheckConsistency() const;

  std::string ToString() const;
  
  inline double predictive_probability(topic_t floor_id,
                                       int type,
                                       double parent_probability,
                                       double discount,
                                       double concentration) const;
  inline std::pair<double, double> cache_probability(topic_t cache_id,
                                                       int type,
                                                       double discount,
                                                       double concentration) const;
  inline void FillInPredictives(const std::vector<double>& parent_predictives,
                                int type,
                                const LambdaManagerInterface& lmanager,
                                int depth,
                                const HPYParameter& hpy_parameter,
                                std::vector<double>& predictives) const;
  inline void FillInPredictivesNonGraphical(
      const std::vector<double>& parent_paredictives,
      int type,
      int depth,
      const HPYParameter& hpy_parameter,
      std::vector<double>& predictives) const;


  
  int customers(topic_t floor_id, int type) const {
    auto it = type2internal_.find(type);
    return it != type2internal_.end() ? (*it).second.customers(floor_id) : 0;
  }
  int tables(topic_t floor_id, int type) const {
    auto it = type2internal_.find(type);
    return it != type2internal_.end() ? (*it).second.tables(floor_id) : 0;
  }
  int labeled_customers(topic_t floor_id, int type, topic_t label) const {
    auto it = type2internal_.find(type);
    return it != type2internal_.end() ? (*it).second.labeled_customers(floor_id, label) : 0;
  }
  int labeled_tables(topic_t floor_id, int type, topic_t label) const {
    auto it = type2internal_.find(type);
    return it != type2internal_.end() ? (*it).second.labeled_tables(floor_id, label) : 0;
  }
  int floor_sum_customers(topic_t floor_id) const {
    auto it = floor2c_t_.find(floor_id);
    return it != floor2c_t_.end() ? (*it).second.first : 0;
  }
  int floor_sum_tables(topic_t floor_id) const {
    auto it = floor2c_t_.find(floor_id);
    return it != floor2c_t_.end() ? (*it).second.second : 0;
  }
  std::map<int, int> floor_table_histogram(topic_t floor_id, bool cache = false) const {
    std::map<int, int> ret;
    for (auto& internal : type2internal_) {
      internal.second.table_histogram(floor_id, cache, ret);
    }
    return ret;
  }
  std::vector<int> table_vector() const {
    std::vector<int> ret;
    for (auto& internal : type2internal_) {
      auto floor_tv = internal.second.table_vector();
      std::copy(floor_tv.begin(), floor_tv.end(), std::back_inserter(ret));
    }
    return ret;
  }
  double logjoint(const HPYParameter& hpy_parameter, int depth) const;
  double logjoint_lambda(double gamma) const { return table_restaurant_.logjoint(gamma); }
  
  int global_labeled_tables() const { return table_restaurant_.num_global_customers(); };
  int local_labeled_tables() const { return table_restaurant_.num_local_customers(); };
  double fractional_local_labeled_tables() const {
    return table_restaurant_.fractional_local_customers();
  }
  double fractional_global_labeled_tables() const {
    return table_restaurant_.fractional_global_customers();
  }
  int global_labeled_table_tables() const { return table_restaurant_.num_global_tables(); }
  int local_labeled_table_tables() const { return table_restaurant_.num_local_tables(); }

  int& stop_customers() { return num_stop_customers_; }
  int& pass_customers() { return num_pass_customers_; }
  int stop_customers() const { return num_stop_customers_; }
  int pass_customers() const { return num_pass_customers_; }

  std::map<int, std::vector<topic_t> > type2topics() const;

  const boost::container::flat_map<topic_t, std::pair<int, int> >& floor2c_t() { return floor2c_t_; }
  const boost::container::flat_map<topic_t, std::pair<int, int> >& cache2c_t() { return floor2c_t(); }
  InternalRestaurant<topic_t>& internal(int type) { return type2internal_[type]; }
  boost::container::flat_map<int, InternalRestaurant<topic_t> >& type2internal() {
    return type2internal_;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "type2internal_" << std::endl;
    for (auto& kv : type2internal_) {
      int type = kv.first;
      auto& internal = kv.second;
      ss << "type=" << type << ":" << std::endl;
      ss << internal.to_string() << std::endl;
    }

    ss << "floor2c_t_" << std::endl;
    for (auto& kv : floor2c_t_) {
      int k = kv.first;
      ss << "k=" << k << ", # customers=" << kv.second.first << ", # tables=" << kv.second.second << std::endl;
    }
    return ss.str();
  }
  
 private:
  boost::container::flat_map<int, InternalRestaurant<topic_t> > type2internal_;
  boost::container::flat_map<topic_t, std::pair<int, int> > floor2c_t_;
  //boost::container::flat_map<topic_t, std::pair<int, int> > cache2c_t_;

  // buffer
  static std::vector<int> tmp_c_;
  
  HistogramTableRestaurant table_restaurant_;

  int num_stop_customers_;
  int num_pass_customers_;
  
  friend class pfi::data::serialization::access;
  template <class Archive>
  void serialize(Archive& ar) {
    ar & MEMBER(type2internal_)
        & MEMBER(floor2c_t_)
        & MEMBER(table_restaurant_)
        & MEMBER(num_stop_customers_)
        & MEMBER(num_pass_customers_);
  }
};

double Restaurant::predictive_probability(topic_t floor_id,
                                          int type,
                                          double parent_probability,
                                          double discount,
                                          double concentration) const {
  int cw = 0;
  int tw = 0;
  int c = 0;
  int t = 0;

  auto section_it = type2internal_.find(type);
  if (section_it != type2internal_.end()) {
    cw = (*section_it).second.customers(floor_id);
    tw = (*section_it).second.tables(floor_id);
  }
  auto floor_it = floor2c_t_.find(floor_id);
  if (floor_it != floor2c_t_.end()) {
    c = (*floor_it).second.first;
    t = (*floor_it).second.second;
  }
  return ComputePYPPredictive(cw, tw, c, t, parent_probability, discount, concentration);
}
std::pair<double, double> Restaurant::cache_probability(topic_t /*cache_id*/,
                                                        int /*type*/,
                                                        double /*discount*/,
                                                        double /*concentration*/) const {
  return {0,0};
  // auto cache_it = cache2c_t_.find(cache_id);
  // if (cache_it == cache2c_t_.end()) {
  //   return {0, 1};
  // }
  // double c = (*cache_it).second.first;
  // double t = (*cache_it).second.second;
  // double cw = 0;
  // double tw = 0;
  // auto section_it = type2internal_.find(type);
  // if (section_it != type2internal_.end()) {
  //   auto& sections = (*section_it).second.cache_sections_;
  //   auto it = sections.find(cache_id);
  //   if (it != sections.end()) {
  //     cw = (*it).second.customers;
  //     tw = (*it).second.tables;
  //   }
  // }
  // return {(cw - tw * discount) / (concentration + c),
  //       (concentration + discount * t) / (concentration + c)};
}
void Restaurant::FillInPredictives(const std::vector<double>& parent_predictives,
                                   int type,
                                   const LambdaManagerInterface& lmanager,
                                   int depth,
                                   const HPYParameter& hpy_parameter,
                                   std::vector<double>& predictives) const {
  auto floor_it = floor2c_t_.begin();
  auto type_it = type2internal_.find(type);
  predictives[0] = parent_predictives[0];
  if (floor2c_t_.empty()) {
    for (size_t i = 1; i < predictives.size(); ++i) {
      double lambda = lmanager.lambda(i, depth);
      predictives[i] = lambda * parent_predictives[i] + (1 - lambda) * predictives[0];
    }
    return;
  }
  if ((*floor_it).first == 0) {
    auto& c_t = (*floor_it).second;
    predictives[0] *= (hpy_parameter.concentration(depth, 0) + hpy_parameter.discount(depth, 0) * c_t.second) / (c_t.first + hpy_parameter.concentration(depth, 0));
    if (type_it != type2internal_.end()) {
      auto& sections = (*type_it).second.sections_;
      if (!sections.empty()) {
        auto section_begin = sections.begin();
        if ((*section_begin).first == 0) {
          int cw = (*section_begin).second.customers;
          int tw = (*section_begin).second.tables;
          predictives[0] += (cw - hpy_parameter.discount(depth, 0) * tw) / (c_t.first + hpy_parameter.concentration(depth, 0));
        }
      }
    }
  }
  for (size_t i = 1; i < predictives.size(); ++i) {
    double lambda = lmanager.lambda(i, depth);
    predictives[i] = lambda * parent_predictives[i] + (1 - lambda) * predictives[0];
  }
  if ((*floor_it).first == 0) {
    ++floor_it;
  }
  for (; floor_it != floor2c_t_.end(); ++floor_it) {
    auto floor_id = (*floor_it).first;
    auto& c_t = (*floor_it).second;
    tmp_c_[floor_id] = c_t.first;
    predictives[floor_id] *=
        (hpy_parameter.concentration(depth, floor_id)
         + hpy_parameter.discount(depth, floor_id) * c_t.second)
        / (c_t.first + hpy_parameter.concentration(depth, floor_id));
  }
  if (type_it == type2internal_.end()) return;
  auto& sections = (*type_it).second.sections_;

  auto section_it = sections.begin();
  if (section_it != sections.end() && (*section_it).first == 0) {
    ++section_it;
  }
  for (; section_it != sections.end(); ++section_it) {
    auto floor_id = (*section_it).first;
    auto& section = (*section_it).second;
    int cw = section.customers;
    int tw = section.tables;
    predictives[floor_id] +=
        (cw - hpy_parameter.discount(depth, floor_id) * tw)
        / (tmp_c_[floor_id] + hpy_parameter.concentration(depth, floor_id));
  }
}
inline void Restaurant::FillInPredictivesNonGraphical(
    const std::vector<double>& parent_predictives,
    int type,
    int depth,
    const HPYParameter& hpy_parameter,
    std::vector<double>& predictives) const {
  for (size_t i = 0; i < parent_predictives.size(); ++i) {
    predictives[i] = parent_predictives[i];
  }
  for (auto floor_it = floor2c_t_.begin(); floor_it != floor2c_t_.end(); ++floor_it) {
    auto floor_id = (*floor_it).first;
    auto& c_t = (*floor_it).second;
    tmp_c_[floor_id] = c_t.first;
    predictives[floor_id] *=
        (hpy_parameter.concentration(depth, floor_id)
         + hpy_parameter.discount(depth, floor_id) * c_t.second)
        / (c_t.first + hpy_parameter.concentration(depth, floor_id));
  }
  auto type_it = type2internal_.find(type);
  if (type_it == type2internal_.end()) return;

  auto& sections = (*type_it).second.sections_;
  auto section_it = sections.begin();
  for (; section_it != sections.end(); ++section_it) {
    auto floor_id = (*section_it).first;
    auto& section = (*section_it).second;
    auto cw = section.customers;
    auto tw = section.tables;
    predictives[floor_id] +=
        (cw - hpy_parameter.discount(depth, floor_id) * tw)
        / (tmp_c_[floor_id] + hpy_parameter.concentration(depth, floor_id));
  }
}

inline double ComputePYPPredictive(int cw,
                                   int tw,
                                   int c,
                                   int t,
                                   double parent_probability,
                                   double discount,
                                   double concentration) {
  if (c == 0) {
    return parent_probability;
  } else {
    return (cw - discount * tw +
            (concentration + discount * t) * parent_probability) /
        (c + concentration);
  }
}

} // namespace topiclm

#endif /* _TOPICLM_RESTAURANT_HPP_ */
