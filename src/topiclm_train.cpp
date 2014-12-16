#include <string>
#include <pficommon/system/time_util.h>
#include <pficommon/data/string/utility.h>
#include "cmdline.h"
#include "random_util.hpp"
#include "topiclm_model.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"

using namespace std;
using namespace pfi::system::time;

topiclm::ReadConfig read_config(const cmdline::parser& p) {
  auto conf = topiclm::ReadConfig();
  std::vector<std::string> conv_strs = topiclm::split_strip(p.get<std::string>("word_converters"));
    
  std::vector<topiclm::WordConverterType> convs(conv_strs.size());
  for (size_t i = 0; i < convs.size(); ++i) {
    convs[i] = topiclm::WordConverterType(stoi(conv_strs[i]));
  }
  conf.conv_types = convs;
  conf.unk_converter_type = topiclm::UnkConverterType(p.get<int>("unk_converter"));
  conf.unk_handler_type = topiclm::UnkHandlerType(p.get<int>("unk_handler"));
  conf.unprocess_with_stream = p.get<int>("unk_stream_start");
  conf.unk_threshold = p.get<int>("unk_threshold");
  conf.unk_type = p.get<std::string>("unk_type");
  conf.format = topiclm::TrainFileFormat(p.get<int>("format"));
  return conf;
}

class TwoToOneTemplatureManager : public TemplatureManager {
 public:
  TwoToOneTemplatureManager(int iterations, int cooling_size)
      : iterations_(iterations),
        cooling_size_(cooling_size),
        templature_(2.0) {}
  virtual void CalcTemplature(int sample_idx) {
    if (sample_idx > cooling_size_) {
      templature_ = 1.0;
    } else {
      templature_ = - 1.0 / (1 + exp(-0.1 * (sample_idx - (cooling_size_ / 2)))) + 2.0;
    }
    assert(templature_ >= 1.0);
  }
  virtual double templature() { return templature_; }
 private:
  int iterations_;
  int cooling_size_;
  double templature_;
};

int main(int argc, char** argv) {
  cmdline::parser p;
  p.add<string>("file", 'f', "training file", true);
  p.add<int>("format", 'F', "file format (0=one file where a document is segmented by blank line,1=list of files each is treated as a document)", false, 0);
  p.add<string>("model", 'm', "directory name where output/log will be stored", true);
  
  // p.add<double>("concentration", 'c', "initial concentration", false, 0.1);
  // p.add<double>("discount", 'D', "initial discount", false, 0.5);
  // p.add<double>("lambda_a", 'A', "parameter a of beta prior of lambda", false, 1);
  // p.add<double>("lambda_b", 'B', "parameter b of beta prior of lambda", false, 1);
  // p.add<double>("lambda_c", 'C', "lambda concentration", false, 10);
  // p.add<double>("alpha_1", '1', "topic dirichlet prior", false, 10);
  // p.add<double>("alpha_0", '0', "hyper of base measure of topics", false, 1);
  
  p.add<int>("order", 'O', "ngram order, -1 for infinite gram", false, 3);
  p.add<int>("num_topics", 'K', "number of topics", true);
  p.add<int>("burn-ins", 'b', "number of iterations", false, 500);
  p.add<int>("num-samples", 'n', "number of samples after burn-in", false, 0);
  p.add<int>("interval", 'i', "number of iterations in each interval of samples after burn-in", false, 20);
  p.add<int>("alpha_start", 'a', "start point of the resampling alpha", false, 1);
  
  // p.add<int>("table_step", 's', "table label resampling step", false, 0);  
  // p.add<int>("alpha_method", 'x', "alpha sampling method(0=uniform across topics; 1=nonuniform", false, 1);
  // p.add<int>("hpylm_method", 'y', "hpylm sampling method(0=uniform across topics; 1=nonuniform", false, 0);
  // p.add<int>("lambda_type", 'l', "lambda type(0=wood; 1=hierarchical)", false, 1);
  
  // p.add<int>("init", 'I', "initialization method(0=random; 1=sequential; 2=partially)", false, 2);
  
  // p.add<double>("prior_pass", 'P', "parameter a of beta prior of stop prob", false, 1.0);
  // p.add<double>("prior_stop", 'S', "parameter b of beta prior of stop prob", false, 4.0);

  // p.add<int>("init_depth", 'd', "initial depth in random initialization", false, 2);
  p.add<int>("tree_type", 't', "tree type(0=graphical; 1=non_graphical)", false, 1);
  p.add<double>("p_global", 'G', "prior probability of topic 0 at random initialization", false, 0.0);
  p.add<bool>("no_global", 'g', "do not considering global topic (similar to Wallach(2006))", false, false);
  
  // p.add<bool>("no_table_sample", 'T', "do not perform table sampler", false, false);
  p.add<int>("table_based_step", 'V', "table based sampling step", false, 1);

  p.add<int>("max_t_in_block", 'e', "In table-based sampler, if # tables exceeds this, that block will be ignored", false, -1);
  p.add<int>("max_c_in_block", 'E', "In table-based sampler, if # customers exceeds this, that block will be ignored", false, -1);
  p.add<bool>("table_include_root", 'R', "visit all tables in the root node, or skip (0=skip; 1=visit)", false, 1);
  p.add<int>("seed", 'A', "random seed", false, -1);
  
  p.add<string>("word_converters", 'c', "list of word converters to apply for each word (ex: -c \"0 1\") (0=lower casing all words; 1=replace all number charactors to # (ex: 12,345=>##,###))", false, "");
  p.add<int>("unk_converter", 'u', "How to convert an unknown token? (0=replace with unk_type; 1=replace with a signature of a surface (e.g., vexing -> UNK-ing; NOTE: English spcific))", false, 0);
  p.add<string>("unk_type", 'T', "a type assigned for unknown token (used only when unk_converter=0)", false, "__unk__");
  p.add<int>("unk_handler", 'H', "When a token is recognized as an unknown token? (0=do nothing; 1=all tokens which counts are below unk_threshold; 2=nothing for first `unk_stream_start` sentences, then, 10% of new words are recognized as unknown)", false, 1);
  
  p.add<int>("unk_stream_start", 's', "When `unk_handler`=2, some new words in sentences which is after this number of sentences are treated as unknown", false, 10000);
  p.add<int>("unk_threshold", 'U', "(used only when `unk_converter`=0)", false, 1);

  p.parse_check(argc, argv);

  double prior_pass = 1.0;
  double prior_stop = 0.0;
  int order = p.get<int>("order");
  if (order == -1) {
    prior_pass = 1.0;
    prior_stop = 4.0;
    order = 8; // 8-gram is a defualt maximum value to achieve infinity-gram
  }
  
  // int init_depth = p.get<int>("init_depth");
  // if (init_depth >= p.get<int>("order") || p.get<double>("prior_stop") == 0.0) {
  //   init_depth = p.get<int>("order") - 1;
  // }
  
  bool table_sample = true;
  try {
    //topiclm::temp_manager.reset(new TwoToOneTemplatureManager(p.get<int>("iterations"), 200));
    if (p.get<int>("seed") == -1) {
      topiclm::init_rnd();
    } else {
      topiclm::init_rnd(p.get<int>("seed"));
    }
    StartLogging(argv, p.get<string>("model"));

    int num_burnins = p.get<int>("burn-ins");
    int interval = p.get<int>("interval");
    int num_samples = num_burnins + p.get<int>("num-samples") * interval;

    double lambda_a = 1; // prior to go to topics other than global
    double lambda_b = 1; // prior to go to the global topic

    double p_global = p.get<double>("p_global");
    auto tree_type = topiclm::TreeType(p.get<int>("tree_type"));
    
    if (p.get<bool>("no_global")) {
      cerr << "no_global flag detected: The model get reduced to HPYTM (without global topic)" << endl;
      p_global = 0;
      lambda_b = 0;
      tree_type = topiclm::kGraphical; // this is for some technical reason; non-global model is only compatible with graphical tree
    }
    
    topiclm::HpyLdaModel<topiclm::HpyLdaSampler> model(
        lambda_a,
        lambda_b,
        10.0, // p.get<double>("lambda_c"),
        prior_pass,
        prior_stop,
        0.5,
        0.1,
        1.0, // p.get<double>("alpha_0"),
        10.0, // p.get<double>("alpha_1"),
        p.get<int>("num_topics"),
        order,
        topiclm::kHierarchical,
        tree_type,
        read_config(p));

    std::cerr << "\ntraining model setting:" << std::endl;
    std::cerr << "--------------------" << std::endl;
    std::cerr << model.status();
    std::cerr << "--------------------\n" << std::endl;

    int alpha_method = 1; // p.get<int>("alpha_method");
    int hpylm_method = 0; // p.get<int>("hpylm_method");
    
    model.ReadTrainFile(p.get<string>("file"));
    
    model.SetSampler();
    model.SetAlphaSampler(topiclm::HyperSamplerType(alpha_method));
    model.SetHpySampler(topiclm::HyperSamplerType(hpylm_method));

    auto& sampler = model.sampler();

    sampler.set_table_based_sampler(p.get<int>("max_t_in_block"),
                                    p.get<int>("max_c_in_block"),
                                    p.get<bool>("table_include_root"));
    
    int table_based_step = p.get<int>("table_based_step");
    if (table_based_step == 0 || p.get<int>("max_t_in_block") == 0 || p.get<int>("num_topics") == 1) {
      table_sample = false;
    }

    std::cerr << "\nlearning setting:" << std::endl;
    std::cerr << "--------------------" << std::endl;
    std::cerr << " # sampling for burn-in: " << num_burnins << std::endl;
    std::cerr << " after that, samples " << p.get<int>("num-samples") << " models every " << interval << "iterations" << std::endl;
    std::cerr << " Table-based sampler:" << std::endl;
    std::cerr << "  Running table-based sampler?: " << (table_sample ? "yes" : "no") << std::endl;
    if (table_sample) {
      std::cerr << "  For every " << table_based_step << " iterations" << std::endl;
      std::cerr << "  Maximum # tables for one block: ";
      if (p.get<int>("max_t_in_block") == -1) std::cerr << "no limitation" << std::endl;
      else std::cerr << p.get<int>("max_t_in_block") << std::endl;
      std::cerr << "  Maximum # customers for one block: ";
      if (p.get<int>("max_c_in_block") == -1) std::cerr << "no limitation" << std::endl;
      else std::cerr << p.get<int>("max_c_in_block") << std::endl;      
    }
    std::cerr << "--------------------\n" << std::endl;

    int init_depth = order - 1;
    if (order == 8) { // infinite gram
      init_depth = 2;
    }
    sampler.InitializeInRandom(init_depth, true, p_global);
    
    double begin = get_clock_time();
    for (int i = 1; i <= num_samples; ++i) {
      //topiclm::temp_manager->CalcTemplature(i);
      double ll = sampler.RunOneIteration(i, table_sample);
      
      double end = get_clock_time();
      LOG("ll") << (end - begin) << "\t" << ll << endl;
      if (i >= num_burnins && (i - num_burnins) % interval == 0) {
        model.SaveModels(p.get<string>("model"), i);
      }
    }
    cerr << "\nsampling done!" << endl;
  } catch (const string& what) {
    cerr << what << endl;
    return 1;
  } catch (char const* what) {
    cerr << what << endl;
    return 1;
  }
}
