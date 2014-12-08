#ifndef _HPY_LDA_LAMBDA_MANAGER_HPP_
#define _HPY_LDA_LAMBDA_MANAGER_HPP_

#include <vector>
#include <set>
#include <sstream>

namespace hpy_lda {

class Node;
class DfsPathIterator;
struct LambdaParameter;

class LambdaManagerInterface {
 public:
  virtual ~LambdaManagerInterface();
  virtual void CalcLambdaPath(const std::vector<Node*>& node_path,
                              int target_depth) = 0;
  virtual void CalcLambdaPathFractionary(const std::vector<Node*>&, int) {}
  virtual void AddNewTable(const std::vector<Node*>& node_path,
                           int topic,
                           int depth,
                           bool is_global) = 0;
  virtual void AddNewTableAtRandom(const std::vector<Node*>& node_path,
                                   int topic,
                                   int depth,
                                   bool is_global) = 0;
  virtual void AddNewTableFractionary(const std::vector<Node*>&, int) {}
  virtual void RemoveTable(const std::vector<Node*>& node_path,
                           int topic,
                           int depth,
                           bool is_global) = 0;
  
  virtual std::string PrintDepth2Tables(
      const std::vector<std::set<Node*> >& depth2nodes) const = 0;
  virtual std::string AnalyzeLambdaPath(
      const std::vector<Node*>& node_path, int target_depth) const = 0;
  virtual std::vector<std::pair<double, std::vector<int> > >
  GetLambdaSortedNgrams(DfsPathIterator it) = 0;

  virtual double lambda(int floor, int depth) const = 0;
  virtual std::vector<std::vector<int> >& topic2global_tables() {
    return dummy_v_;
  }
  virtual std::vector<std::vector<int> >& topic2local_tables() {
    return dummy_v_;
  }
  virtual const std::vector<double>& lambda_path() const = 0;
  virtual const std::vector<double>& global_lambda_path() const = 0;
  virtual double root_lambda() const = 0;
 private:
  std::vector<std::vector<int> > dummy_v_;
};

class LambdaManager : public LambdaManagerInterface {
 public:
  LambdaManager(const LambdaParameter& lambda_parameter, int num_topics, int ngram_order)
      : depth2topic2global_tables_(ngram_order, std::vector<int>(num_topics + 1, 0)),
        depth2topic2local_tables_(ngram_order, std::vector<int>(num_topics + 1, 0)),
        lambda_parameter_(lambda_parameter),
        depth2topic2lambda_(ngram_order, std::vector<double>(num_topics + 1, 0.0)) ,
        dummy_lambda_path_(ngram_order) {}
  virtual ~LambdaManager();
  virtual void CalcLambdaPath(const std::vector<Node*>& node_path, int target_depth);
  virtual void AddNewTable(
      const std::vector<Node*>& /*node_path*/, int topic, int depth, bool is_global) {
    if (is_global) {
      ++depth2topic2global_tables_[depth][topic];
    } else {
      ++depth2topic2local_tables_[depth][topic];
    }
  }
  virtual void AddNewTableAtRandom(
      const std::vector<Node*>& node_path, int topic, int depth, bool is_global) {
    AddNewTable(node_path, topic, depth, is_global);
  }
  virtual void RemoveTable(
      const std::vector<Node*>& /*node_path*/, int topic, int depth, bool is_global) {
    if (is_global) {
      --depth2topic2global_tables_[depth][topic];
    } else {
      --depth2topic2local_tables_[depth][topic];
    }
  }
  
  virtual std::string PrintDepth2Tables(
      const std::vector<std::set<Node*> >& depth2nodes) const;
  virtual std::string AnalyzeLambdaPath(
      const std::vector<Node*>& node_path, int target_depth) const;
  virtual std::vector<std::pair<double, std::vector<int> > >
  
  GetLambdaSortedNgrams(DfsPathIterator it);
  virtual double lambda(int floor, int depth) const {
    return depth2topic2lambda_[depth][floor];
  }
  virtual std::vector<std::vector<int> >& topic2global_tables() {
    return depth2topic2global_tables_;
  }
  virtual std::vector<std::vector<int> >& topic2local_tables() {
    return depth2topic2local_tables_;
  }
  virtual const std::vector<double>& lambda_path() const { return dummy_lambda_path_; }
  virtual const std::vector<double>& global_lambda_path() const { return dummy_lambda_path_; }
  virtual double root_lambda() const;
  
 private:
  std::vector<std::vector<int> > depth2topic2global_tables_;
  std::vector<std::vector<int> > depth2topic2local_tables_;
  const LambdaParameter& lambda_parameter_;
  // buffer
  std::vector<std::vector<double> > depth2topic2lambda_;
  std::vector<double> dummy_lambda_path_;
};

class HierarchicalLambdaManager : public LambdaManagerInterface {
 public:
  HierarchicalLambdaManager(const LambdaParameter& lambda_parameter, int ngram_order);
  virtual ~HierarchicalLambdaManager();
  virtual void CalcLambdaPath(const std::vector<Node*>& node_path,
                              int target_depth);
  virtual void CalcLambdaPathFractionary(const std::vector<Node*>& node_path,
                                         int target_depth);
  virtual void AddNewTable(
      const std::vector<Node*>& node_path, int topic, int depth, bool is_global);
  virtual void AddNewTableAtRandom(
      const std::vector<Node*>& node_path, int topic, int depth, bool is_global);
  virtual void AddNewTableFractionary(
      const std::vector<Node*>& node_path, int depth);
  virtual void RemoveTable(
      const std::vector<Node*>& node_path, int topic, int depth, bool is_global);
  
  virtual std::string PrintDepth2Tables(
      const std::vector<std::set<Node*> >& depth2nodes) const;
  virtual std::string AnalyzeLambdaPath(
      const std::vector<Node*>& node_path, int target_depth) const;
  virtual std::vector<std::pair<double, std::vector<int> > >
  GetLambdaSortedNgrams(DfsPathIterator it);
  
  virtual double lambda(int floor, int depth) const {
    if (floor == 0) return 1;
    return lambda_path_[depth];
  }
  virtual const std::vector<double>& lambda_path() const { return lambda_path_; }
  virtual const std::vector<double>& global_lambda_path() const { return global_lambda_path_; }
  virtual double root_lambda() const;

 protected:
  const LambdaParameter& lambda_parameter_;
  // buffer
  std::vector<double> lambda_path_;
  std::vector<double> global_lambda_path_; // for fractional caluculation only
  
  mutable std::vector<std::pair<int, int> > depth2global_local_num_tables_;
};


} // hpy_lda

#endif /* _HPY_LDA_LAMBDA_MANAGER_HPP_ */
