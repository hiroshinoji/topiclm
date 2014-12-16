#include <vector>
#include <unordered_map>
#include "floor_sampler.hpp"

#include <gtest/gtest.h>

using namespace topiclm;
using namespace std;

double CalcArrangementProbNumerDirect(double discount,
                                      double concentration,
                                      int t_block,
                                      int t_restaurant) {
  double direct = 1.0;
  for (int t = 0; t < t_block; ++t) {
    direct *= (concentration + discount * (t_restaurant + t));
  }
  return direct;
}
double CalcRisingFactorialDirect(double concentration,
                                 double c_block,
                                 double c_restaurant) {
  double direct = 1.0;
  for (int c = 0; c < c_block; ++c) {
    direct *= (concentration + c_restaurant + c);
  }
  return direct;
}

double CalcArrangementProbDirect(double discount,
                                 double concentration,
                                 int c_block,
                                 int t_block,
                                 int c_restaurant,
                                 int t_restaurant) {
  double direct = 0.0;
  for (int t = 0; t < t_block; ++t) {
    direct += log(concentration + discount * (t_restaurant + t));
  }
  for (int c = 0; c < c_block; ++c) {
    direct -= log(concentration + c_restaurant + c);
  }
  return exp(direct);
  // return CalcArrangementProbNumerDirect(discount, concentration, t_block, t_restaurant)
  //     / CalcRisingFactorialDirect(concentration, c_block, c_restaurant);
}

TEST(arrangement_prob_numer_cahce, calc) {
  double discount = 0.2;
  double concentration = 10.0;
  int table_already = 3;
  ArrangementProbNumeratorCache num_cache;
  num_cache.Init(table_already, discount, concentration);
  
  num_cache.get(5);
  EXPECT_FLOAT_EQ(num_cache.get(0), 0.0);
  for (size_t i = 1; i <= 5; ++i) {
    double direct = CalcArrangementProbNumerDirect(discount, concentration, i, table_already);
    EXPECT_FLOAT_EQ(num_cache.get(i), log(direct));
  }
}

TEST(rising_factorial_cache, calc) {
  RisingFactorialCache fact_cache;
  int n = 10;
  double a = 10.0;
  fact_cache.Init(n, a);

  fact_cache.get(5);
  EXPECT_FLOAT_EQ(fact_cache.get(0), 0.0);
  for (size_t i = 1; i <= 5; ++i) {
    double direct = CalcRisingFactorialDirect(a, i, n);
    EXPECT_FLOAT_EQ(fact_cache.get(i), log(direct));
  }
}


TEST(seating_arrangement_prob_calculator, calc) {
  double discount = 0.2;
  double concentration = 10.0;
  
  SeatingArrangementProbCalculator calc(discount, concentration);

  vector<pair<int, int> > restaurant_settings = {{10,3}, {13,5}, { 8,2}, { 8,5}, { 0, 0}};
  vector<pair<int, int> > block_settings      = {{ 0,0}, { 1,1}, {10,3}, { 3,1}, { 5, 5}, {100,30}};
  //vector<pair<int, int> > block_settings      = {{ 0,0}, { 1,1}, { 3, 1}, { 5, 5}};
  
  for (auto& r : restaurant_settings) {
    for (auto& b : block_settings) {
      double direct = CalcArrangementProbDirect(discount, concentration, b.first, b.second, r.first, r.second);
      double cache = calc.log_p(b.first, b.second, r.first, r.second);
      EXPECT_FLOAT_EQ(cache, log(direct));
    }
  }
}

// this method rely on built-in lgamma functions, which I used previously in the floor_sampler
double CalcTopicPriorDirect(const vector<double>& alpha,
                            size_t k,
                            size_t old_k,
                            const unordered_map<int, int>& doc2move_customers,
                            const vector<pair<int, vector<int> > >& doc2topic_count) {
  double alpha_sum = accumulate(alpha.begin(), alpha.end(), 0.0);
  double direct = 0;
  for (auto& doc2move_customer : doc2move_customers) {
    int j = doc2move_customer.first;
    double a = alpha[k];
    int b_j = doc2move_customer.second;
    double N_j = alpha_sum + doc2topic_count[j].first - b_j;
    double n_jk = a + doc2topic_count[j].second[k];
    if (k == old_k) {
      n_jk -= b_j;
      EXPECT_TRUE(n_jk >= 0);
    }
    direct += lgamma(N_j) - lgamma(N_j + b_j) + lgamma(n_jk + b_j) - lgamma(n_jk);
  }
  return direct;
}

// create data for test, number of documents = 3, number of topics = 4
// let's assume moving customers from k=1
unordered_map<int, int> trivial_doc2move_customers() {
  unordered_map<int, int> doc2move_customers;
  doc2move_customers[0] = 1; doc2move_customers[1] = 30; doc2move_customers[2] = 5;
  return doc2move_customers;
}
vector<pair<int, vector<int> > > trivial_doc2topic_count() {
  vector<pair<int, vector<int> > > doc2topic_count(3);
  doc2topic_count[0].first = 100;
  doc2topic_count[0].second = {0, 50, 20, 30};
  doc2topic_count[1].first = 100;
  doc2topic_count[1].second = {0, 35, 15, 50};
  doc2topic_count[2].first = 100;
  doc2topic_count[2].second = {0, 20, 20, 60};
  return doc2topic_count;
}

TEST(topic_prior_calculator, calc) {
  vector<double> alpha = {0, 0.1, 0.1, 0.1};
  TopicPriorCalculator calc(alpha);

  auto doc2move_customers = trivial_doc2move_customers();
  auto doc2topic_count = trivial_doc2topic_count();

  int from_k = 1;
  for (int k = 1; k <= 3; ++k) {
    double cache = calc.log_p(k, from_k, doc2move_customers, doc2topic_count);
    double direct = CalcTopicPriorDirect(alpha, k, from_k, doc2move_customers, doc2topic_count);
    EXPECT_FLOAT_EQ(cache, direct);
  }
}

TEST(floor_sampler, reset_cache) {
  
}
