#include "context_tree_analyzer.hpp"
#include "context_tree_manager.hpp"
#include "lambda_manager.hpp"
#include "util.hpp"
#include "node_util.hpp"
#include "restaurant_manager.hpp"
#include "log.hpp"

using namespace std;

namespace hpy_lda {

ContextTreeAnalyzer::ContextTreeAnalyzer(ContextTreeManager& ct_manager,
                                         const pfi::data::intern<std::string>& intern)
    : ct_manager_(ct_manager), intern_(intern) {}

void ContextTreeAnalyzer::AnalyzeRestaurant(
    const vector<string>& ngram, std::ostream& os) const {
  int unk_id = intern_.key2id_nogen(kUnkKey);
  vector<int> id_ngram = ToIdNgram(ngram);
  int type = id_ngram[id_ngram.size() - 1];
  if (type == unk_id) {
    os << "unknown type : " << type;
    return;
  }
  int word_depth = ct_manager_.WalkTreeNoCreate(id_ngram, id_ngram.size() - 2, 0);
  cerr << word_depth << endl;
  os << "context: " << endl;
  for (int i = word_depth; i >= 0; --i) {
    os << ngram[ngram.size() - 1 - i] << " ";
  }
  os << endl;

  ct_manager_.rmanager().CalcDepth2TopicPredictives(type, word_depth);
  auto& depth2topic_predictives = ct_manager_.rmanager().depth2topic_predictives();

  os << "probs:" << endl;
  for (size_t i = 0; i < depth2topic_predictives[0].size(); ++i) {
    os << " topic" << i << ": ";
    for (int j = 0; j <= word_depth; ++j) {
      os << depth2topic_predictives[j][i] << " ";
    }
    os << endl;
  }
  auto& node_path = ct_manager_.current_node_path();
  os << ct_manager_.lmanager_->AnalyzeLambdaPath(node_path, word_depth);
  os << "restaurants:" << endl;
  for (int i = 0; i <= word_depth; ++i) {
    auto& restaurant = node_path[i]->restaurant();
    os << "depth: " << i << endl;
    os << " depth: " << i << endl;
    for (int j = 0; j < ct_manager_.num_topics_ + 1; ++j) {
      os << "  floor" << j << ": ";
      os << "c: "
         << restaurant.customers(j, type) << " / "
         << restaurant.floor_sum_customers(j) << ", ";
      os << "t: "
         << restaurant.tables(j, type)
         << "(" << restaurant.labeled_tables(j, type, 0) << ")" << " / "
         << restaurant.floor_sum_tables(j) << endl;
    }
  }
}

void ContextTreeAnalyzer::AnalyzeTopic(const vector<string>& ngram, ostream& os) const {
  size_t num_displays = 5;
  int unk_id = intern_.key2id_nogen(kUnkKey);
  vector<int> id_ngram = ToIdNgram(ngram);
  int type = id_ngram[id_ngram.size() - 1];
  if (type == unk_id) {
    os << "unknown word : " << type << endl;
    return;
  }
  int word_depth = ct_manager_.WalkTreeNoCreate(id_ngram, id_ngram.size() - 1, 0);
  cerr << word_depth << endl;
  os << "context: " << endl;
  for (int i = word_depth - 1; i >= 0; --i) {
    os << ngram[ngram.size() - 1 - i] << " ";
  }
  os << endl;

  auto& node_path = ct_manager_.current_node_path();
  auto& restaurant = node_path[word_depth]->restaurant();
  auto type2topics = restaurant.type2topics();
  vector<vector<pair<double, int> > > topic2topical_words(ct_manager_.num_topics_ + 1);

  auto& depth2topic_predictives = ct_manager_.rmanager().depth2topic_predictives();
  ct_manager_.CalcLambdaPath(word_depth);
  for (auto& type2topic : type2topics) {
    int type = type2topic.first;
    auto& topics = type2topic.second;
    ct_manager_.rmanager().CalcGlobalPredictivePath(type, word_depth);
    double p_t_w = 0;
    for (auto topic : topics) {
      if (topic != 0) {
        ct_manager_.rmanager().CalcLocalPredictivePath(type,
                                            topic,
                                            word_depth);
      }
      p_t_w = depth2topic_predictives[word_depth][topic];
      auto& topical_words = topic2topical_words[topic];
      if (topical_words.size() < num_displays
          || topical_words[topical_words.size() - 1].first < p_t_w) {
        topical_words.emplace_back(p_t_w, type);
      }
      if (topical_words.size() > num_displays) {
        sort(topical_words.begin(), topical_words.end(), greater<pair<double, int> > ());
        topical_words.pop_back();
      }
    }
  }
  for (size_t i = 0; i < topic2topical_words.size(); ++i) {
    if (topic2topical_words[i].empty()) continue;
    os << "topic" << i << ":" << endl;
    for (auto& topical_word : topic2topical_words[i]) {
      os << " " << getWord(topical_word.second, intern_) << " " << topical_word.first << endl;
    }
  }
}
void ContextTreeAnalyzer::AnalyzeLambdaToNgrams(std::ostream& os) const {
  auto lambda_sorted_id_ngrams = ct_manager_.lmanager_->GetLambdaSortedNgrams(ct_manager_.ct_.GetDfsPathIterator());
  vector<pair<double, string> > lambda_sorted_ngrams(lambda_sorted_id_ngrams.size());
  for (size_t i = 0; i < lambda_sorted_id_ngrams.size(); ++i) {
    lambda_sorted_ngrams[i].first = lambda_sorted_id_ngrams[i].first;
    string ngram;
    for (int id : lambda_sorted_id_ngrams[i].second) {
      ngram += getWord(id, intern_) + " ";
    }
    lambda_sorted_ngrams[i].second = ngram;
  }
  for (size_t i = 0; i < lambda_sorted_ngrams.size(); ++i) {
    os << lambda_sorted_ngrams[i].second << "\t" << lambda_sorted_ngrams[i].first << endl;
  }
}

bool addProbNgrams(double p,
                   double p_loc,
                   double p_gl,
                   double p_n,
                   double lambda,
                   const std::vector<Node*>& node_path,
                   int depth,
                   const pfi::data::intern<string>& intern,
                   int type,
                   int K,
                   vector<pair<tuple<double, double, double, double, double>, string> >& prob_ngrams) {
  if ((!prob_ngrams.empty() && get<0>(prob_ngrams.back().first) < p)
      || (int)prob_ngrams.size() < K) {
    string phrase;
    for (int j = depth; j > 0; --j) {
      phrase += getWord(node_path[j]->type(), intern) + " ";
    }
    phrase += getWord(type, intern);
    prob_ngrams.push_back(make_pair(make_tuple(p, p_loc, p_gl, p_n, lambda), phrase));
    sort(prob_ngrams.begin(), prob_ngrams.end(), greater<pair<tuple<double,double,double,double, double>, string> >());
    if ((int)prob_ngrams.size() > K) {
      prob_ngrams.pop_back();
      assert((int)prob_ngrams.size() == K);
    }
    return true;
  }
  return false;
}

void PrintTopicalNgrams(const vector<vector<pair<tuple<double,double,double,double,double>, string> > >& topic2topical_ngrams, ostream& os) {
  for (size_t i = 0; i < topic2topical_ngrams.size(); ++i) {
    os << "topic: " << i << endl;
    for (auto& prob_ngram : topic2topical_ngrams[i]) {
      os << prob_ngram.second << "\t";
      os << get<0>(prob_ngram.first) << " ";
      os << get<1>(prob_ngram.first) << " ";
      os << get<2>(prob_ngram.first) << " ";
      os << get<3>(prob_ngram.first) << " ";
      os << get<4>(prob_ngram.first) << endl;
    }
  }
}

void ContextTreeAnalyzer::CalcTopicalNgrams(int K, ostream& os) const {
  vector<vector<pair<tuple<double, double, double, double, double>, string> > > topic2topical_ngrams(ct_manager_.num_topics_ + 1);
  int ngram_order = ct_manager_.parameters_.ngram_order();
  for (auto it = ct_manager_.ct_.GetDfsNodeIterator(); it.HasMore(); ++it) {
    auto node = *it;
    int depth = node->depth();
    ct_manager_.UpTreeFromLeaf(node, depth);
    ct_manager_.CalcStopPriorPath(depth, ngram_order - 1);

    double p_n = ct_manager_.stop_prior_path()[depth];
    
    auto& node_path = ct_manager_.current_node_path();
    auto& restaurant = node_path[depth]->restaurant();

    auto type2topics = restaurant.type2topics();

    ct_manager_.CalcLambdaPath(depth);
    double lambda = ct_manager_.lambda_path()[depth];
    
    auto& depth2topic_predictives = ct_manager_.rmanager().depth2topic_predictives();
    for (auto& type2topic : type2topics) {
      int type = type2topic.first;
      ct_manager_.rmanager().CalcGlobalPredictivePath(type, depth);
      double p_gl = depth2topic_predictives[depth][0];
      for (auto topic : type2topic.second) {
        if (topic != 0) {
          ct_manager_.rmanager().CalcLocalPredictivePath(type, topic, depth);
        }
        double p_loc = depth2topic_predictives[depth][topic];
        double p_t_w = p_loc * p_n;
        addProbNgrams(p_t_w, p_loc, p_gl, p_n, lambda, node_path, depth, intern_, type, K, topic2topical_ngrams[topic]);
      }
    }
  }
  PrintTopicalNgrams(topic2topical_ngrams, os);
}
void ContextTreeAnalyzer::CalcRelativeTopicalNgrams(int K, ostream& os) const {
  vector<vector<pair<tuple<double, double, double, double, double>, string> > > topic2topical_ngrams(ct_manager_.num_topics_ + 1);
  int ngram_order = ct_manager_.parameters_.ngram_order();
  for (auto it = ct_manager_.ct_.GetDfsNodeIterator(); it.HasMore(); ++it) {
    auto node = *it;
    int depth = node->depth();
    ct_manager_.UpTreeFromLeaf(node, depth);
    ct_manager_.CalcStopPriorPath(depth, ngram_order - 1);

    double p_n = ct_manager_.stop_prior_path()[depth];

    auto& node_path = ct_manager_.current_node_path();
    auto& restaurant = node_path[depth]->restaurant();

    auto type2topics = restaurant.type2topics();

    ct_manager_.CalcLambdaPath(depth);
    double lambda = ct_manager_.lambda_path()[depth];
    auto& depth2topic_predictives = ct_manager_.rmanager().depth2topic_predictives();
    for (auto& type2topic : type2topics) {
      int type = type2topic.first;
      ct_manager_.rmanager().CalcGlobalPredictivePath(type, depth);
      double global_p = depth2topic_predictives[depth][0];
      for (auto topic : type2topic.second) {
        if (topic == 0) continue;
        ct_manager_.rmanager().CalcLocalPredictivePath(type, topic, depth);
        double p_loc = depth2topic_predictives[depth][topic];
        double p_t_w = p_loc * p_n;
        addProbNgrams(p_t_w / global_p, p_loc, global_p, p_n, lambda, node_path, depth, intern_, type, K, topic2topical_ngrams[topic]);
      }
    }
  }
  PrintTopicalNgrams(topic2topical_ngrams, os);
}
int ContextTreeAnalyzer::CountNodes() const {
  return ct_manager_.ct_.CountNodes();
}
void ContextTreeAnalyzer::CheckInternalConsistency() const {
  int node_num = CountNodes();
  cerr << "node num: " << node_num << endl;
  int cnt = 0;
  int sum_observed_customers = 0;
  for (auto node_it = ct_manager_.ct_.GetDfsNodeIterator(); node_it.HasMore(); ++node_it) {
    if (++cnt % 10 == 0) {
      cerr << "node " << setw(5) << cnt << " / " << node_num << "\r";
    }
    auto& node = **node_it;
    auto& restaurant = node.restaurant();
    auto& children = node.children();
    if (children.empty()) {
      auto type2topics = restaurant.type2topics();
      for (auto& type2topic : type2topics) {
        int type = type2topic.first;
        for (size_t i = 0; i < type2topic.second.size(); ++i) {
          auto topic = type2topic.second[i];
          int global_labeled_tables = restaurant.labeled_tables(topic, type, 0);
          sum_observed_customers +=
              restaurant.customers(topic, type) - global_labeled_tables;
        }
      }
      continue;
    }
    vector<map<topic_t, int> > topic_type2loc_tables(ct_manager_.num_topics_ + 1);
    for (auto& child : children) {
      auto& child_restaurant = child.second->restaurant();
      auto type2topics = child_restaurant.type2topics();
      for (auto& type2topic : type2topics) {
        int type = type2topic.first;
        for (size_t i = 0; i < type2topic.second.size(); ++i) {
          auto topic = type2topic.second[i];
          int global_labeled_tables = child_restaurant.labeled_tables(topic, type, 0);
          int local_labeled_tables = child_restaurant.tables(topic, type) - global_labeled_tables;
          topic_type2loc_tables[topic][type] += local_labeled_tables;
        }
      }
    }
    auto type2topics = restaurant.type2topics();
    for (auto& type2topic : type2topics) {
      int type = type2topic.first;
      for (size_t i = 0; i < type2topic.second.size(); ++i) {
        auto topic = type2topic.second[i];
        if (topic == 0) continue;
        int global_labeled_tables = restaurant.labeled_tables(topic, type, 0);
        topic_type2loc_tables[0][type] += global_labeled_tables;
      }
    }
    int restaurant_customers = 0;
    int estimated_restaurant_customers = 0;
    for (auto& type2topic : type2topics) {
      int type = type2topic.first;
      for (size_t i = 0; i < type2topic.second.size(); ++i) {
        auto topic = type2topic.second[i];
        restaurant_customers += restaurant.customers(topic, type);
        estimated_restaurant_customers += topic_type2loc_tables[topic][type];
      }
    }
    assert(restaurant_customers - restaurant.stop_customers() == estimated_restaurant_customers);
  }
}

void ContextTreeAnalyzer::LogAllNgrams() const {
  auto& depth2nodes = ct_manager_.GetDepth2Nodes();
  vector<Node*> node_path(depth2nodes.size());
  int n = 0;
  for (size_t i = 0; i < depth2nodes.size(); ++i) {
    for (auto node : depth2nodes[i]) {
      util::UpTreeFromLeaf(node, i, node_path);
      stringstream ss;
      ss << n++ << "\t";
      for (size_t j = 0; j < i; ++j) {
        size_t k = i - j; // i, i-1, ..., 0
        if (k != i) ss << " "; // not first word
        ss << intern_.id2key(node_path[k]->type());
      }
      LOG("ngram") << ss.str() << endl;
    }
  }
}

}
