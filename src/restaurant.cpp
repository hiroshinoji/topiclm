#include "restaurant.hpp"
#include "random_util.hpp"

using namespace std;

namespace topiclm {

vector<double> HistogramTableRestaurant::table_probs_;// = vector<double>();
vector<int> Restaurant::tmp_c_;// = vector<int>();

void Restaurant::SetBufferSize(size_t s) { tmp_c_.resize(s); }

pair<AddRemoveResult, topic_t> Restaurant::AddCustomer(
      topic_t floor_id,
      int type,
      double local_parent_probability,
      const std::vector<double>& global_parent_pdf,
      double lambda,
      const std::vector<double>& label_priors,
      double discount,
      double concentration) {
  auto& target_internal = type2internal_[type];
  auto& target_stat = floor2c_t_[floor_id];
  ++target_stat.first;
  
  auto add_result = target_internal.AddCustomer(floor_id,
                                                local_parent_probability,
                                                global_parent_pdf,
                                                lambda,
                                                label_priors,
                                                discount,
                                                concentration,
                                                target_stat.second);
  if (add_result.first != TableUnchanged) {
    ++target_stat.second;
  }
  return add_result;
}
AddRemoveResult Restaurant::AddCustomerNewTable(topic_t floor_id, int type, double lambda) {
  auto& target_internal = type2internal_[type];
  auto& target_stat = floor2c_t_[floor_id];
  ++target_stat.first;
  auto add_result = target_internal.AddCustomerNewTable(floor_id, lambda);
  assert(add_result != TableUnchanged);
  ++target_stat.second;

  return add_result;
}
pair<AddRemoveResult, topic_t> Restaurant::RemoveCustomer(topic_t floor_id, int type, bool cache) {
  auto& target_internal = type2internal_[type];
  //auto& target_c_t = cache ? cache2c_t_ : floor2c_t_;
  auto& target_c_t = floor2c_t_;
  auto& target_stat = target_c_t[floor_id];
  assert(--target_stat.first >= 0);
  auto remove_result = target_internal.RemoveCustomer(floor_id, cache);

  if (remove_result.first != TableUnchanged) {
    if (--target_stat.second == 0) {
      assert(target_stat.first == 0);
      EraseAndShrink(target_c_t, floor_id);
    }
  }
  return remove_result;
}
void Restaurant::ChangeTableLabelAtRandom(topic_t floor_id, int type, topic_t old_label, topic_t new_label) {
  auto& target_internal = type2internal_[type];
  target_internal.ChangeTableLabelAtRandom(floor_id, old_label, new_label);
}
void Restaurant::ChangeTableLabel(topic_t floor_id, int type, int customers, topic_t old_label, topic_t new_label) {
  auto& target_internal = type2internal_[type];
  target_internal.ChangeTableLabel(floor_id, customers, old_label, new_label);
}

void Restaurant::CombineSectionToWord(topic_t floor_id,
                                      int type,
                                      const std::shared_ptr<Word>& word_ptr,
                                      bool cache) {
  auto& target_internal = type2internal_[type];
  target_internal.CombineSectionToWord(floor_id, word_ptr, cache);
}
void Restaurant::SeparateWordFromSection(topic_t floor_id,
                                         int type,
                                         const std::shared_ptr<Word>& word_ptr,
                                         bool cache) {
  auto& target_internal = type2internal_[type];
  target_internal.SeparateWordFromSection(floor_id, word_ptr, cache);
}
void Restaurant::AddCustomerToCache(topic_t /*cache_id*/, int /*type*/, topic_t /*topic*/, double /*discount*/) {
  // auto& target_internal = type2internal_[type];
  // auto& target_stat = cache2c_t_[cache_id];
  // ++target_stat.first;
  // if (topic == -1) { // do not create new 
  //   target_internal.AddCacheCustomerToExistingTables(cache_id, discount);
  // } else {
  //   target_internal.AddCacheCustomerToNewTable(cache_id, topic);
  //   ++target_stat.second;
  // }
}
SampleTableInfo Restaurant::SampleTableAtRandom(topic_t floor_id, int type) {
  auto it = type2internal_.find(type);
  if (it == type2internal_.end()) {
    return SampleTableInfo();
  }
  return (*it).second.SampleTableAtRandom(floor_id);
}
void Restaurant::ChangeTablesFloor(topic_t floor_id, int type, topic_t label, const vector<int>& table_customers, topic_t new_floor_id) {
  auto& old_stat = floor2c_t_[floor_id];

  int move_customers = accumulate(table_customers.begin(), table_customers.end(), 0);
  old_stat.first -= move_customers;
  if ((old_stat.second -= table_customers.size()) == 0) {
    assert(old_stat.first == 0);
    EraseAndShrink(floor2c_t_, floor_id);
  }
  auto& new_stat = floor2c_t_[new_floor_id];
  new_stat.first += move_customers;
  new_stat.second += table_customers.size();
  
  auto& target_internal = type2internal_[type];
  target_internal.ChangeTablesFloor(floor_id, new_floor_id, label, table_customers);
}

void Restaurant::CheckConsistency() const {
  map<topic_t, pair<int, int> > floor2c_t;
  for (auto& internal : type2internal_) {
    for (auto& section : internal.second.sections_) {
      auto floor = section.first;
      int c = 0;
      int t = 0;
      auto& histogram = section.second.customer_histogram;
      for (auto& label2histo : histogram) {
        for (auto& bucket : label2histo.second) {
          c += bucket.first * bucket.second;
          t += bucket.second;
        }
      }
      assert(c == section.second.customers);
      assert(t == section.second.tables);
      floor2c_t[floor].first += c;
      floor2c_t[floor].second += t;
    }
  }
  assert(floor2c_t.size() == floor2c_t_.size());
  for (auto& datum : floor2c_t) {
    assert(datum.second.first == floor2c_t_.at(datum.first).first);
    assert(datum.second.second == floor2c_t_.at(datum.first).second);
  }
}

map<int, vector<topic_t> > Restaurant::type2topics() const {
  map<int, vector<topic_t> > type2topics;
  for (auto& internal : type2internal_) {
    int type = internal.first;
    type2topics[type] = internal.second.type_vector();
  }
  return type2topics;
}

double Restaurant::logjoint(const HPYParameter& hpy_parameter, int depth) const {
  double ll = 0;
  for (auto& floor_c_t : floor2c_t_) {
    topic_t k = floor_c_t.first;
    double a = hpy_parameter.discount(depth, k);
    double b = hpy_parameter.concentration(depth, k);

    int c_k = floor_c_t.second.first;
    int t_k = floor_c_t.second.second;

    double log_denom = 0;
    double log_newtable = 0;
    double log_oldtable = 0;

    log_denom = lgamma(b) - lgamma(b + c_k); // 1/b * 1/b+1 * 1/b+2 ... * 1/(b+c_k-1)
    for (int t = 0; t < t_k; ++t) {
      log_newtable += log(b + t * a);
    }
    for (auto& internal : type2internal_) {
      auto k_it = internal.second.sections_.find(k);
      if (k_it == internal.second.sections_.end()) continue;
      auto& section = (*k_it).second; // section is not empty at the word 'w'
      auto& histogram = section.customer_histogram;
      for (auto& label2histo : histogram) {
        for (auto& bucket : label2histo.second) {
          int c_wkl = bucket.first;  // num of customers at some table
          int t_wkl = bucket.second; // num of tables s.t. around customers == c_wkl
          for (int i = 0; i < t_wkl; ++i) {
            for (int j = 1; j < c_wkl; ++j) {
              log_oldtable += log(j - a);
            }
          }
        }
      }
    }
    ll += log_denom + log_newtable + log_oldtable;
    // if (ll > 0) {
    //   cerr << "ll " << ll << endl;
    //   cerr << "log_denom " << log_denom << endl;
    //   cerr << "log_newtable " << log_newtable << endl;
    //   cerr << "log_oldtable " << log_oldtable << endl;
    //   cerr << endl;
    //   cerr << "current k = " << k << endl;
    //   cerr << "restaurant stats " << endl;
    //   cerr << to_string() << endl;
    // }
  }
  //assert(ll <= 0);
  return ll;
}

};

