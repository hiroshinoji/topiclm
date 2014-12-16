#ifndef _TOPICLM_TOPICLM_MODEL_HPP_
#define _TOPICLM_TOPICLM_MODEL_HPP_

#include <memory>
#include <cassert>
#include "parameters.hpp"
#include "topiclm.hpp"
#include "unigram_rescaling.hpp"
#include "config.hpp"
#include "serialization.hpp"
#include "document_manager.hpp"
#include "particle_filter_document_manager.hpp"
#include "io_util.hpp"

namespace topiclm {

template <class SamplerType>
class HpyLdaModel {
 public:
  HpyLdaModel() {}
  HpyLdaModel(HpyLdaModel&&) = default;
  HpyLdaModel(double lambda_a,
              double lambda_b,
              double lambda_c,
              double prior_pass,
              double prior_stop,
              double discount,
              double concentration,
              double alpha_0,
              double alpha_1,
              int num_topics,
              int ngram_order,
              LambdaType lambda_type,
              TreeType tree_type,
              ReadConfig config)
  : lambda_type_(lambda_type),
    tree_type_(tree_type),
    config_(config),
    
    parameters_(lambda_a,
                lambda_b,
                lambda_c,
                prior_pass,
                prior_stop,
                discount,
                concentration,
                alpha_0,
                alpha_1,
                num_topics,
                ngram_order),
    dmanager_(num_topics, ngram_order) {}
  
  void ReadTrainFile(const std::string& fn) {
    dmanager_.Read(reader(fn));
  }
  
  ParticleFilterDocumentManager GetPFDocumentManager(int num_particles) {
    return ParticleFilterDocumentManager(dmanager_.intern(),
                                         num_particles,
                                         parameters_.topic_parameter().num_topics,
                                         parameters_.ngram_order());
  }
  void SetSampler() {
    sampler_.reset(new SamplerType(lambda_type_, tree_type_, dmanager_, parameters_));
  }
  void SetAlphaSampler(HyperSamplerType sample_type) {
    parameters_.SetAlphaSampler(sample_type);
  }
  void SetHpySampler(HyperSamplerType sample_type) {
    parameters_.SetHpySampler(sample_type);
  }
  void SaveModels(const std::string& dir, int i) {
    std::string model_fn = dir + "/model/" + std::to_string(i) + ".out";
    SaveModel(model_fn, *this);
    std::string topic_prefix = dir + "/topic/" + std::to_string(i);
    std::string assign_fn = topic_prefix + ".assign";
    std::string count_fn = topic_prefix + ".count";
    {
      std::ofstream ofs(assign_fn);
      dmanager_.OutputTopicAssign(ofs);
    }
    {
      std::ofstream ofs(count_fn);
      dmanager_.OutputTopicCount(ofs);
    }
  }
  
  SamplerType& sampler() { return *sampler_; }

  std::shared_ptr<Reader> reader(const std::string& fn) {
    auto word_convs = word_converters();
    auto unk_converter = std::shared_ptr<WordConverter>(unk_converter_ptr());

    auto unk_handler = std::shared_ptr<UnkWordHandler>(
        unk_handler_ptr(fn, word_convs, unk_converter));

    return std::shared_ptr<Reader>(reader_ptr(fn, word_convs, unk_handler));
  }
  /**
   * When fn is omitted, SentenceReader is returned
   */ 
  std::shared_ptr<Reader> reader_for_test(const std::string& fn = "") {
    config_.unk_handler_type = kDict;
    return reader(fn);
  }
  
  pfi::data::intern<std::string>& intern() { return dmanager_.intern(); }

  bool empty_intern() const {
    auto& dict = dmanager_.intern();
    if (dict.empty()) return true;
    else {
      if (dict.size() == 1 && dict.key2id_nogen(kEosKey) != -1) return true;
      else return false;
    }
  }

  std::string status() const {
    std::stringstream ss;
    ss << " # topics: " << parameters_.topic_parameter().num_topics << std::endl;
    
    ss << " # tree type: ";
    if (tree_type_ == kGraphical) ss << "graphical PYP" << std::endl;
    else if (tree_type_ == kNonGraphical) ss << "non-graphical PYP" << std::endl;

    ss << " n-gram order: ";
    int order = parameters_.ngram_order();
    if (order == 8) ss << "infinity" << std::endl;
    else ss << order << std::endl;

    ss << " model name: ";
    if (tree_type_ == kGraphical) {
      if (parameters_.topic_parameter().alpha[0] == 0) {
        ss << "HPYTM (without global topic)" << std::endl;
      } else {
        ss << "DHPYTM (global topic is used for back-off)" << std::endl;
      }
    } else if (tree_type_ == kNonGraphical) {
      ss << "cHPYTM (global topic collects general words)" << std::endl;
    }
    return ss.str();
  }
  
 private:
  LambdaType lambda_type_;
  TreeType tree_type_;
  ReadConfig config_;
  
  std::unique_ptr<SamplerType> sampler_;
  Parameters parameters_;
  DocumentManager dmanager_;

  std::vector<std::shared_ptr<WordConverter> > word_converters() const {
    std::vector<std::shared_ptr<WordConverter> > converters;
    if (!config_.conv_types.empty()) {
      std::cerr << "selected word converters:" << std::endl;
    }
      
    for (auto& t : config_.conv_types) {
      if (t == kLower) {
        std::cerr << " lower-converer" << std::endl;
        converters.emplace_back(std::make_shared<LowerConverter>());
      } else if (t == kNumber) {
        std::cerr << " number-killer" << std::endl;
        converters.emplace_back(std::make_shared<NumberKiller>());
      }
    }
    
    return converters;
  }
  
  WordConverter* unk_converter_ptr() const {
    if (config_.unk_converter_type == kNormal) return new ToUnkConverter(config_.unk_type);
    else return new BerkeleySignatureExtractor();
  }
  
  UnkWordHandler* unk_handler_ptr(const std::string& fn,
                                  const std::vector<std::shared_ptr<WordConverter> >& converters,
                                  const std::shared_ptr<WordConverter> unk_converter) {
    auto& dict = dmanager_.intern();
    
    if (config_.unk_handler_type == kNone) {
      std::cerr << "unknown handler: none" << std::endl;
      
      return new NoneUnkHandler();
    } else if (config_.unk_handler_type == kDict) {
      std::cerr << "unknown handler: dictionary based (threshold: "
                << config_.unk_threshold << ")" << std::endl;

      if (empty_intern()) {
        std::cerr << "Dictionary is empty; now constructing..." << std::endl;
        auto temporal_unk_handler = std::make_shared<NoneUnkHandler>();
        auto reader = std::unique_ptr<Reader>(
            reader_ptr(fn, converters, temporal_unk_handler));
        reader->BuildDictionary(dict, config_.unk_threshold);
        std::cerr << "done." << std::endl;
      } else {
        std::cerr << "Using loaded dictionary (# types: " << dict.size() << ")" << std::endl;
      }
      return new UnkWordHandlerWithDict(dict, unk_converter);
    } else {
      std::cerr << "unknown handler: stream based (start after "
                << config_.unprocess_with_stream << " sentences)" << std::endl;
      
      return new StreamUnkWordHandler(dict, unk_converter, config_.unprocess_with_stream);
    }
  }
  
  Reader* reader_ptr(const std::string& fn,
                     const std::vector<std::shared_ptr<WordConverter> >& converters,
                     const std::shared_ptr<UnkWordHandler> unk_handler) {
    if (fn.empty()) {
      return new SentenceReader(converters, unk_handler);
    } else if (config_.format == kOneDoc) {
      return new OneDocReader(fn, converters, unk_handler);
    } else {
      return new MultiDocReader(fn, converters, unk_handler);
    }
  }

  friend class pfi::data::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar) {
    if (ar.is_read) {
      int lambda_type;
      ar & lambda_type;      
      int tree_type;
      ar & tree_type;
      lambda_type_ = LambdaType(lambda_type);
      tree_type_ = TreeType(tree_type);
    } else {
      int lambda_type = int(lambda_type_);
      ar & lambda_type;
      int tree_type = int(tree_type_);
      ar & tree_type;
    }
    ar & MEMBER(config_)
        & MEMBER(parameters_)
        & MEMBER(dmanager_);
    if (ar.is_read) {
      SetSampler();
    }
    ar & MEMBER(*sampler_);
  }
};

template <class SamplerType>
inline void SaveModel(const std::string& fn, HpyLdaModel<SamplerType>& model) {
  std::ofstream ofs(fn);
  if (!ofs) {
    throw "cannot open model file " + fn;
  }
  pfi::data::serialization::binary_oarchive oa(ofs);
  oa << model;
  std::string end_flag = "end";
  oa << end_flag;
}

template <class SamplerType>
inline HpyLdaModel<SamplerType> LoadModel(const std::string& fn) {
  HpyLdaModel<SamplerType> model;
  std::ifstream ifs(fn);
  if (!ifs) {
    throw "cannot read model file " + fn;
  }
  pfi::data::serialization::binary_iarchive ia(ifs);

  ia >> model;

  std::string end_flag;
  ia >> end_flag;
  assert(end_flag == "end");

  std::cerr << "model load done." << std::endl;
  
  std::cerr << "\nsetting of loaded model:" << std::endl;
  std::cerr << "--------------------" << std::endl;
  std::cerr << model.status();
  std::cerr << "--------------------\n" << std::endl;
  
  return model;
}

} // topiclm
  
#endif /* _TOPICLM_TOPICLM_MODEL_HPP_ */
