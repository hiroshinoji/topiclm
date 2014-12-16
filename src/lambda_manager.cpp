#include "lambda_manager.hpp"
#include "restaurant.hpp"
#include "node.hpp"
#include "context_tree.hpp"
#include "parameters.hpp"

using namespace std;

namespace topiclm {

LambdaManagerInterface::~LambdaManagerInterface() {}

LambdaManager::~LambdaManager() {}

void LambdaManager::CalcLambdaPath(const vector<Node*>& /*node_path*/, int target_depth) {
  for (int i = 0; i <= target_depth; ++i) {
    for (size_t j = 0; j < depth2topic2lambda_[i].size(); ++j) {
      if (j == 0) {
        depth2topic2lambda_[i][j] = 1;
      } else {
        double a = depth2topic2local_tables_[i][j] + lambda_parameter_.a;
        double b = depth2topic2global_tables_[i][j] + lambda_parameter_.b;
        depth2topic2lambda_[i][j] = a / (a + b);
      }
    }
  }
}
string LambdaManager::PrintDepth2Tables(
    const vector<set<Node*> >& /*depth2wnodes*/) const {
  stringstream ss;
  ss << "global_tables\tlocal_tables" << endl;
  for (size_t i = 0; i < depth2topic2local_tables_.size(); ++i) {
    for (size_t j = 0; j < depth2topic2local_tables_[i].size(); ++j) {
      ss << "topic" << j << "[" << i << "] " << depth2topic2local_tables_[i][j] << ", " << depth2topic2global_tables_[i][j] << endl;
    }
  }
  return ss.str();
}
string LambdaManager::AnalyzeLambdaPath(
    const vector<Node*>& /*node_path*/, int target_depth) const {
    stringstream ss;
  ss << "lambda path:";
  for (size_t i = 0; i < depth2topic2lambda_[0].size(); ++i) {
    ss << " topic " << i << ": ";
    for (int j = 0; j <= target_depth; ++j) {
      ss << depth2topic2lambda_[j][i] << " ";
    }
  }
  ss << "each node tables:";
  ss << "  global / local" << endl;
  for (int j = 0; j <= target_depth; ++j) {
    ss << " depth " << j << ":" << endl;
    for (size_t i = 0; i < depth2topic2global_tables_[j].size(); ++i) {
      ss << "  t" << i << ": "
         << depth2topic2global_tables_[i][j] << " / "
         << depth2topic2local_tables_[i][j] << endl;
    }
  }
  return ss.str();
}
vector<pair<double, vector<int> > >
LambdaManager::GetLambdaSortedNgrams(DfsPathIterator /*it*/) {
  return vector<pair<double, vector<int> > >();
}
double LambdaManager::root_lambda() const {
  return (double)lambda_parameter_.a
      / (double)(lambda_parameter_.a + lambda_parameter_.b);
}

HierarchicalLambdaManager::HierarchicalLambdaManager(
    const LambdaParameter& lambda_parameter, int ngram_order)
    : lambda_parameter_(lambda_parameter),
      lambda_path_(ngram_order),
      global_lambda_path_(ngram_order),
      depth2global_local_num_tables_(ngram_order) {}

HierarchicalLambdaManager::~HierarchicalLambdaManager() {}

void HierarchicalLambdaManager::CalcLambdaPath(const std::vector<Node*>& node_path,
                                               int target_depth) {
  double lambda = lambda_parameter_.a
      / (lambda_parameter_.a + lambda_parameter_.b);
  for (int j = 0; j <= target_depth; ++j) {
    double c = lambda_parameter_.c[j];
    auto& restaurant = node_path[j]->restaurant();
    double local_tables = restaurant.local_labeled_tables();
    double global_tables = restaurant.global_labeled_tables();
    lambda = (local_tables + c * lambda) / (local_tables + global_tables + c);
    lambda_path_[j] = lambda;
  }
  for (size_t j = target_depth + 1; j < lambda_path_.size(); ++j) {
    lambda_path_[j] = lambda_path_[j - 1];
  }
}

void HierarchicalLambdaManager::CalcLambdaPathFractionary(
    const vector<Node*>& node_path,
    int target_depth) {
  double lambda = root_lambda();
  double global_lambda = 1 - lambda;
  for (int j = 0; j <= target_depth; ++j) {
    double c = lambda_parameter_.c[j];
    auto& restaurant = node_path[j]->restaurant();
    double local_tables = restaurant.local_labeled_tables();
    double fractional_local_tables = restaurant.fractional_local_labeled_tables();
    double global_tables = restaurant.global_labeled_tables();
    double fractional_global_tables = restaurant.fractional_global_labeled_tables();
    lambda = (local_tables + fractional_local_tables + c * lambda)
        / (local_tables + fractional_local_tables + global_tables + c);
    global_lambda = (global_tables + fractional_global_tables + c * global_lambda)
        / (global_tables + fractional_global_tables + local_tables + c);
    lambda_path_[j] = lambda;
    global_lambda_path_[j] = global_lambda;
  }
  for (size_t j = target_depth + 1; j < lambda_path_.size(); ++j) {
    lambda_path_[j] = lambda_path_[j - 1];
    global_lambda_path_[j] = global_lambda_path_[j - 1];
  }
}

void HierarchicalLambdaManager::AddNewTable(
    const vector<Node*>& node_path, int /*topic*/, int depth, bool is_global) {
  for (int i = depth; i >= 0; --i) {
    auto& restaurant = node_path[i]->restaurant();
    double parent_prob = 0;
    if (i == 0) {
      parent_prob = lambda_parameter_.a
          / (lambda_parameter_.a + lambda_parameter_.b);
    } else if (is_global) {
      parent_prob = 1 - lambda_path_[i - 1];
    } else {
      parent_prob = lambda_path_[i - 1];
    }
    auto add_result = restaurant.AddTable(is_global, parent_prob, lambda_parameter_.c[i]);
    if (add_result == TableUnchanged) {
      break;
    }
    assert(add_result == LocalTableChanged);
  }
}

void HierarchicalLambdaManager::AddNewTableAtRandom(
    const vector<Node*>& node_path, int /*topic*/, int depth, bool is_global) {
  for (int i = depth; i >= 0; --i) {
    auto& restaurant = node_path[i]->restaurant();
    restaurant.AddTableNewTable(is_global, 1.0);
  }
}
void HierarchicalLambdaManager::AddNewTableFractionary(
    const vector<Node*>& node_path, int depth) {
  double enter_customer = 1;
  double global_enter_customer = 1;
  for (int i = depth; i >= 0; --i) {
    double local_parent = i == 0 ? root_lambda() : lambda_path_[i - 1];
    double global_parent =  i == 0 ? 1 - root_lambda() : global_lambda_path_[i - 1];
    auto& restaurant = node_path[i]->restaurant();
    enter_customer = restaurant.AddTableNewTableFractionary(
        false,
        enter_customer,
        local_parent,
        lambda_parameter_.c[i]);
    global_enter_customer = restaurant.AddTableNewTableFractionary(
        true,
        global_enter_customer,
        global_parent,
        lambda_parameter_.c[i]);
  }
}
void HierarchicalLambdaManager::RemoveTable(
    const vector<Node*>& node_path, int /*topic*/, int depth, bool is_global) {
  for (int i = depth; i >= 0; --i) {
    auto& restaurant = node_path[i]->restaurant();
    auto remove_result = restaurant.RemoveTable(is_global);
    if (remove_result == TableUnchanged) {
      break;
    }
    assert(remove_result == LocalTableChanged);
  }
}

string HierarchicalLambdaManager::PrintDepth2Tables(
    const vector<set<Node*> >& depth2wnodes) const {
  depth2global_local_num_tables_.assign(depth2wnodes.size(), {0,0});
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    for (auto node : depth2wnodes[i]) {
      auto& restaurant = node->restaurant();
      depth2global_local_num_tables_[i].first += restaurant.global_labeled_tables();
      depth2global_local_num_tables_[i].second += restaurant.local_labeled_tables();
    }
  }
  stringstream ss;
  ss << "[ ] global_tables, local_tables" << endl;
  for (size_t i = 0; i < depth2global_local_num_tables_.size(); ++i) {
    ss << "[" << i << "] "
       << depth2global_local_num_tables_[i].first << ", "
       << depth2global_local_num_tables_[i].second << endl;
  }
  
  depth2global_local_num_tables_.assign(depth2wnodes.size(), {0,0});
  for (size_t i = 0; i < depth2wnodes.size(); ++i) {
    for (auto node : depth2wnodes[i]) {
      auto& restaurant = node->restaurant();
      depth2global_local_num_tables_[i].first += restaurant.stop_customers();
      depth2global_local_num_tables_[i].second += restaurant.pass_customers();
    }
  }
  ss << "[ ] stop_customers, pass_customers" << endl;
  for (size_t i = 0; i < depth2global_local_num_tables_.size(); ++i) {
    ss << "[" << i << "] "
       << depth2global_local_num_tables_[i].first << ", "
       << depth2global_local_num_tables_[i].second << endl;
  }
  return ss.str();
}

string HierarchicalLambdaManager::AnalyzeLambdaPath(
    const vector<Node*>& node_path, int target_depth) const {
  stringstream ss;
  ss << "lambda path: ";
  for (int i = 0; i <= target_depth; ++i) {
    ss << lambda_path_[i] << " ";
  }
  ss << endl;
  ss << "each restaurant tables:" << endl;
  ss << " local / global" << endl;
  for (int i = 0; i <= target_depth; ++i) {
    auto node = node_path[i];
    ss << "  " << node->restaurant().local_labeled_tables() << " / "
       << node->restaurant().global_labeled_tables() << endl;
  }
  return ss.str();
}
vector<pair<double, vector<int> > >
HierarchicalLambdaManager::GetLambdaSortedNgrams(DfsPathIterator it)  {
  vector<pair<double, vector<int> > > lambda_sorted_ngrams;
  for ( ; it.HasMore(); ++it) {
    auto& node_path = *it;
    CalcLambdaPath(node_path, node_path.size() - 1);
    double p = lambda_path_[lambda_path_.size() - 1];
    vector<int> ngram;
    for (int i = node_path.size() - 1; i > 0; --i) {
      ngram.push_back(node_path[i]->type());
    }
    lambda_sorted_ngrams.emplace_back(p, ngram);
  }
  sort(lambda_sorted_ngrams.begin(), lambda_sorted_ngrams.end(), greater<pair<double, vector<int> > >());
  return lambda_sorted_ngrams;
}

double HierarchicalLambdaManager::root_lambda() const { 
  return (double)lambda_parameter_.a
      / (double)(lambda_parameter_.a + lambda_parameter_.b);
}

} // topiclm
