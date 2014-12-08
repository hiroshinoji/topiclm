#ifndef _HPY_LDA_TOPIC_SAMPLER_HPP_
#define _HPY_LDA_TOPIC_SAMPLER_HPP_

#include <vector>
#include <algorithm>
#include "random_util.hpp"
#include "parameters.hpp"

namespace hpy_lda {

struct SampleInfo {
  bool cache;
  int depth;
  int topic;
  double p_w;
};

class TopicDepthSamplerInterface {
 public:
  virtual ~TopicDepthSamplerInterface() {}
  virtual void InitWithTopicPrior(const std::pair<int, std::vector<int> >& topic_count,
                                  const std::vector<double>& /*lambda_path*/, int max_depth = -1) = 0;
  virtual void TakeInStopPrior(const std::vector<double>& stop_prior_path) = 0;
  virtual void TakeInLikelihood(const std::vector<std::vector<double> >& likelihoods) = 0;
  virtual void TakeInCache(const std::vector<std::pair<double, double> >& /*cache_path*/) {}
  virtual SampleInfo Sample() = 0;
  virtual int SampleAtRandom(double p_global = 0) const = 0;
  
  double CalcMarginal() const {
    auto& topic_depth_pdf_ = topic_depth_pdf();
    return std::accumulate(topic_depth_pdf_.begin(), topic_depth_pdf_.end(), 0.0);    
  }
  virtual const std::vector<double>& topic_depth_pdf() const = 0;
};

// used for dhpytm
class TopicDepthSampler : public TopicDepthSamplerInterface {
 public:
  TopicDepthSampler(const Parameters& parameters)
      : topic_depth_pdf_(
          (parameters.topic_parameter().num_topics + 1) * parameters.ngram_order()),
        ngram_order_(parameters.ngram_order()),
        num_topics_(parameters.topic_parameter().num_topics),
        topic_parameter_(parameters.topic_parameter()) {}
  virtual ~TopicDepthSampler() {}
  
  virtual void InitWithTopicPrior(const std::pair<int, std::vector<int> >& topic_count,
                                  const std::vector<double>& /*lambda_path*/,
                                  int max_depth = -1) {
    set_current_max_depth(max_depth);
    std::fill(topic_depth_pdf_.begin(), topic_depth_pdf_.end(), 0.0);
    for (int j = 0; j < num_topics_ + 1; ++j) {
      topic_depth_pdf_[j] = (topic_count.second[j] + topic_parameter_.alpha[j])
          / (topic_count.first + topic_parameter_.alpha_1);
    }
    int k = num_topics_ + 1;
    for (int i = 1; i < current_max_depth_; ++i) {
      for (int j = 0; j < num_topics_ + 1; ++j) {
        topic_depth_pdf_[k] = topic_depth_pdf_[j];
        ++k;
      }
    }
  }
  virtual void TakeInStopPrior(const std::vector<double>& stop_prior_path) {
    int k = 0;
    for (int i = 0; i < current_max_depth_; ++i) {
      for (int j = 0; j < num_topics_ + 1; ++j) {
        topic_depth_pdf_[k] *= stop_prior_path[i];
        ++k;
      }
    }
  }
  virtual void TakeInLikelihood(const std::vector<std::vector<double> >& likelihoods) {
    int k = 0;
    for (int i = 0; i < current_max_depth_; ++i) {
      for (int j = 0; j < num_topics_ + 1; ++j) {
        topic_depth_pdf_[k] *= likelihoods[i][j];
        ++k;
      }
    }
  }
  virtual SampleInfo Sample() {
    double p_w = std::accumulate(topic_depth_pdf_.begin(), topic_depth_pdf_.end(), 0.0);
    int sample = random->SampleUnnormalizedPdfRef(topic_depth_pdf_);
    
    return {false, sample / (num_topics_ + 1), sample % (num_topics_ + 1), p_w};
  }
  virtual int SampleAtRandom(double) const {
    return random->SampleUnnormalizedPdf(topic_parameter_.alpha);
  }

  virtual const std::vector<double>& topic_depth_pdf() const { return topic_depth_pdf_; }
  
 protected:
  void set_current_max_depth(int max_depth) {
    if (max_depth == -1) current_max_depth_ = ngram_order_;
    else current_max_depth_ = std::min(max_depth + 1, ngram_order_);
  }
  int current_max_depth_; // max depth 
  std::vector<double> topic_depth_pdf_;
  const int ngram_order_;
  const int num_topics_;
  const DirichletParameter& topic_parameter_;
};

// used for chpytm
class NonGraphicalTopicDepthSampler : public TopicDepthSampler {
 public:
  NonGraphicalTopicDepthSampler(const Parameters& parameters)
      : TopicDepthSampler(parameters) {}
  virtual ~NonGraphicalTopicDepthSampler() {}
  virtual void InitWithTopicPrior(const std::pair<int, std::vector<int> >& topic_count,
                                  const std::vector<double>& lambda_path,
                                  int max_depth = -1) {
    set_current_max_depth(max_depth);
    std::fill(topic_depth_pdf_.begin(), topic_depth_pdf_.end(), 0.0);
    topic_depth_pdf_[0] = 1 - lambda_path[0];
    for (int j = 1; j < num_topics_ + 1; ++j) {
      topic_depth_pdf_[j] = (topic_count.second[j] + topic_parameter_.alpha[j])
          / (topic_count.first + topic_parameter_.alpha_1);
    }
    int k = num_topics_ + 1;
    for (int i = 1; i < current_max_depth_; ++i) {
      topic_depth_pdf_[k] = 1 - lambda_path[i];
      ++k;
      for (int j = 1; j < num_topics_ + 1; ++j) {
        topic_depth_pdf_[k] = lambda_path[i] * topic_depth_pdf_[j];
        ++k;
      }
    }
    for (int j = 1; j < num_topics_ + 1; ++j) {
      topic_depth_pdf_[j] *= lambda_path[0];
    }
  }
  virtual int SampleAtRandom(double p_global) const {
    if (p_global == 0) {
      //return random->NextMult(topic_parameter_.num_topics) + 1;
      return random->NextMult(topic_parameter_.num_topics + 1);
    } else {
      if (random->NextDouble() < p_global) {
        return 0;
      } else {
        return random->NextMult(topic_parameter_.num_topics) + 1;
      }
    }
  }
};

// may be no use ?
class CachedTopicDepthSampler : public TopicDepthSamplerInterface {
 public:
  CachedTopicDepthSampler(const Parameters& parameters)
      : topic_depth_pdf_(
          (parameters.topic_parameter().num_topics + 2) * parameters.ngram_order()),
        ngram_order_(parameters.ngram_order()),
        num_topics_(parameters.topic_parameter().num_topics),
        topic_parameter_(parameters.topic_parameter()) {}


  virtual void InitWithTopicPrior(const std::pair<int, std::vector<int> >& topic_count,
                                  const std::vector<double>& /*lambda_path*/, int) {
    for (int j = 0; j < num_topics_ + 1; ++j) {
      topic_depth_pdf_[j] = (topic_count.second[j] + topic_parameter_.alpha[j])
          / (topic_count.first + topic_parameter_.alpha_1);
    }
    topic_depth_pdf_[num_topics_ + 1] = 1;
    for (int i = 1, k = num_topics_ + 2; i < ngram_order_; ++i, ++k) {
      for (int j = 0; j < num_topics_ + 1; ++j, ++k) {
        topic_depth_pdf_[k] = topic_depth_pdf_[j];
      }
      topic_depth_pdf_[k] = 1;
    }
  }
  virtual void TakeInStopPrior(const std::vector<double>& stop_prior_path) {
    for (int i = 0, k = 0; i < ngram_order_; ++i) {
      for (int j = 0; j < num_topics_ + 2; ++j, ++k) {
        topic_depth_pdf_[k] *= stop_prior_path[i];
      }
    }
  }
  virtual void TakeInLikelihood(const std::vector<std::vector<double> >& likelihoods) {
    for (int i = 0, k = 0; i < ngram_order_; ++i, ++k) {
      for (int j = 0; j < num_topics_ + 1; ++j, ++k) {
        topic_depth_pdf_[k] *= likelihoods[i][j];
      }
    }
  }
  virtual void TakeInCache(const std::vector<std::pair<double, double> >& cache_path) {
    // for (auto p : cache_path) std::cerr << p.first << "," << p.second << " ";
    // std::cerr << std::endl;

    for (int i = 0, k = 0; i < ngram_order_; ++i, ++k) {
      for (int j = 0; j < num_topics_ + 1; ++j, ++k) {
        topic_depth_pdf_[k] *= cache_path[i].second;
      }
      topic_depth_pdf_[k] *= cache_path[i].first;
    }
  }
  virtual SampleInfo Sample() {
    // for (int i = 0, k = 0; i < ngram_order_; ++i) {
    //   for (int j = 0; j < num_topics_ + 2; ++j, ++k) {
    //     std::cerr << topic_depth_pdf_[k] << " ";
    //   }
    //   std::cerr << std::endl;
    // }
    // std::cerr << std::endl;

    double p_w = std::accumulate(topic_depth_pdf_.begin(), topic_depth_pdf_.end(), 0.0);
    int sample = random->SampleUnnormalizedPdfRef(topic_depth_pdf_);
    bool cache = sample > 0 && (sample % (num_topics_ + 2) == num_topics_ + 1);
    return {cache, sample / (num_topics_ + 2), sample % (num_topics_ + 2), p_w};
  }
  virtual int SampleAtRandom(double) const {
    return random->SampleUnnormalizedPdf(topic_parameter_.alpha);
  }

  const std::vector<double>& topic_depth_pdf() const { return topic_depth_pdf_; }
  
 protected:
  std::vector<double> topic_depth_pdf_;
  const int ngram_order_;
  const int num_topics_;
  const DirichletParameter& topic_parameter_;
};

// used for unigram rescaling
class TopicSampler {
 public:
  TopicSampler(const Parameters& parameters)
      : topic_pdf_(parameters.topic_parameter().num_topics + 1),
        topic_parameter_(parameters.topic_parameter()) {}
  void InitWithTopicPrior(const std::pair<int, std::vector<int> >& topic_count) {
    for (size_t j = 0; j < topic_pdf_.size(); ++j) {
      topic_pdf_[j] = (topic_count.second[j] + topic_parameter_.alpha[j])
          / (topic_count.first + topic_parameter_.alpha_1);
    }
  }
  void TakeInLikelihood(const std::vector<double>& likelihood) {
    for (size_t j = 0; j < topic_pdf_.size(); ++j) {
      topic_pdf_[j] *= likelihood[j];
    }
  }
  SampleInfo Sample() {
    double p_w = CalcMarginal();
    int sample = random->SampleUnnormalizedPdfRef(topic_pdf_);
    return {false, 0, sample, p_w};
  }
  double CalcMarginal() const {
    return std::accumulate(topic_pdf_.begin(), topic_pdf_.end(), 0.0);
  }
  int SampleAtRandom() const {
    return random->SampleUnnormalizedPdf(topic_parameter_.alpha);
  }
  const std::vector<double>& topic_pdf() { return topic_pdf_; }
  
 private:
  std::vector<double> topic_pdf_;
  const DirichletParameter& topic_parameter_;
};

}

#endif /* _HPY_LDA_TOPIC_SAMPLER_HPP_ */
