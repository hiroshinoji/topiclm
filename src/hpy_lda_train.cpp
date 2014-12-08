#include <string>
#include <pficommon/system/time_util.h>
#include "cmdline.h"
#include "random_util.hpp"
#include "hpy_lda_model.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"

using namespace std;
using namespace pfi::system::time;

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
  p.add<string>("model", 'm', "directory name where output/log will be stored", true);
  // p.add<double>("concentration", 'c', "initial concentration", false, 0.1);
  // p.add<double>("discount", 'D', "initial discount", false, 0.5);
  // p.add<double>("lambda_a", 'A', "parameter a of beta prior of lambda", false, 1);
  // p.add<double>("lambda_b", 'B', "parameter b of beta prior of lambda", false, 1);
  p.add<double>("lambda_c", 'C', "lambda concentration", false, 10);
  p.add<double>("alpha_1", '1', "topic dirichlet prior", false, 10);
  p.add<double>("alpha_0", '0', "hyper of base measure of topics", false, 1);
  p.add<int>("order", 'O', "ngram order", false, 3);
  p.add<int>("num_topics", 'K', "number of topics", true);
  p.add<int>("burn-ins", 'b', "number of iterations", false, 500);
  p.add<int>("num-samples", 'n', "number of samples after burn-in", false, 20);
  p.add<int>("interval", 'i', "number of iterations in each interval of samples after burn-in", false, 20);
  p.add<int>("alpha_start", 'a', "start point of the resampling alpha", false, 1);
  p.add<int>("table_step", 's', "table label resampling step", false, 0);
  
  p.add<int>("alpha_method", 'x', "alpha sampling method(0=uniform,1=nonuniform,2=global,3=none", false, 1);
  p.add<int>("hpylm_method", 'y', "hpylm sampling method(0=uniform,1=nonuniform,2=global,3=none", false, 0);
  p.add<int>("lambda_type", 'l', "lambda type(0=wood,1=hierarchical)", false, 1);
  p.add<int>("init", 'I', "initialization method(0=random,1=sequential,2=partially)", false, 2);
  p.add<double>("prior_pass", 'P', "parameter a of beta prior of stop prob", false, 1.0);
  p.add<double>("prior_stop", 'S', "parameter b of beta prior of stop prob", false, 4.0);
  
  p.add<int>("init_depth", 'd', "initial depth in random initialization", false, 2);
  p.add<int>("tree_type", 't', "tree type(0=graphical,1=non_graphical)", false, 0);
  p.add<double>("p_global", 'G', "prior probability of topic 0 at random initialization", false, 0.0);
  p.add<bool>("no_global", 'g', "do not considering global topic (similar to Wallach(2006))", false, false);
  p.add<bool>("no_table_sample", 'T', "do not perform table sampler", false, false);
  p.add<int>("table_based_step", 'V', "table based sampling step", false, 1);

  p.add<int>("max_t_in_block", 'e', "In table-based sampler, if # tables exceeds this, that block will be ignored", false, -1);
  p.add<int>("max_c_in_block", 'E', "In table-based sampler, if # customers exceeds this, that block will be ignored", false, -1);
  p.add<bool>("table_include_root", 'R', "visit all tables in the root node, or skip (0=skip,1=visit)", false, 1);
  p.add<int>("seed", 'A', "random seed", false, -1);


  p.parse_check(argc, argv);
  
  int init_depth = p.get<int>("init_depth");
  if (init_depth >= p.get<int>("order") || p.get<double>("prior_stop") == 0.0) {
    init_depth = p.get<int>("order") - 1;
  }
  bool table_sample = !p.get<bool>("no_table_sample");
  try {
    //hpy_lda::temp_manager.reset(new TwoToOneTemplatureManager(p.get<int>("iterations"), 200));
    if (p.get<int>("seed") == -1) {
      hpy_lda::init_rnd();
    } else {
      hpy_lda::init_rnd(p.get<int>("seed"));
    }
    StartLogging(argv, p.get<string>("model"));

    int num_burnins = p.get<int>("burn-ins");
    int interval = p.get<int>("interval");
    int num_samples = num_burnins + p.get<int>("num-samples") * interval;

    double lambda_a = 1;
    double lambda_b = 1;
    double p_global = p.get<double>("p_global");
    if (p.get<bool>("no_global")) {
      lambda_b = 0;
      p_global = 0;

      if (hpy_lda::TreeType(p.get<int>("tree_type")) == hpy_lda::kNonGraphical) {
        throw string("non global model (e.g. HPYTM) can only be activated with tree_type=0.");
      }
    }
    hpy_lda::HpyLdaModel<hpy_lda::HpyLdaSampler> model(
        lambda_a,
        lambda_b,
        p.get<double>("lambda_c"),
        p.get<double>("prior_pass"),
        p.get<double>("prior_stop"),
        0.5,
        0.1,
        p.get<double>("alpha_0"),
        p.get<double>("alpha_1"),
        p.get<int>("num_topics"),
        p.get<int>("order"),
        hpy_lda::LambdaType(p.get<int>("lambda_type")),
        hpy_lda::TreeType(p.get<int>("tree_type")));
    
    model.ReadTrainFile(p.get<string>("file"));
    model.SetSampler();
    model.SetAlphaSampler(hpy_lda::HyperSamplerType(p.get<int>("alpha_method")));
    model.SetHpySampler(hpy_lda::HyperSamplerType(p.get<int>("hpylm_method")));

    auto& sampler = model.sampler();

    sampler.set_table_based_sampler(p.get<int>("max_t_in_block"),
                                    p.get<int>("max_c_in_block"),
                                    p.get<bool>("table_include_root"));
    
    auto ct_analyzer = sampler.GetCTAnalyzer();

    int i = 0;
    int init = p.get<int>("init");
    int table_based_step = p.get<int>("table_based_step");
    if (table_based_step == 0 || p.get<int>("max_t_in_block") == 0 || p.get<int>("num_topics") == 1) {
      table_sample = false;
    }
    
    if (init == 0 || init == 2) {
      sampler.InitializeInRandom(init_depth, init == 0, p_global);
    } else {
      bool do_table_based = table_sample && (i % table_based_step == 0);
      sampler.RunOneIteration(0, do_table_based);
    }
    ct_analyzer.LogAllNgrams();
    
    double begin = get_clock_time();
    for (i = 1; i <= num_samples; ++i) {
      hpy_lda::temp_manager->CalcTemplature(i);
      double ll = sampler.RunOneIteration(i, table_sample);
      
      double end = get_clock_time();
      LOG("ll") << (end - begin) << "\t" << ll << endl;
      if (i >= num_burnins && (i - num_burnins) % interval == 0) {
        model.SaveModels(p.get<string>("model"), i);
      }
    }
    
    // if (i % 50 != 0) {
    //   model.SaveModels(p.get<string>("model"), i);
    // }
    cerr << "\nsampling done!" << endl;
    ct_analyzer.CheckInternalConsistency();
  } catch (const string& what) {
    cerr << what << endl;
    return 1;
  } catch (char const* what) {
    cerr << what << endl;
    return 1;
  }
}
