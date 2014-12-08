#include <string>
#include "cmdline.h"
#include "random_util.hpp"
#include "hpy_lda_model.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"

using namespace std;

int main(int argc, char** argv) {
  cmdline::parser p;
  p.add<string>("file", 'a', "training file", true);
  p.add<string>("model", 'b', "directory name where output/log will be stored", true);
  p.add<double>("concentration", 'c', "initial concentration", false, 0.1);
  p.add<double>("discount", 'd', "initial discount", false, 0.5);
  p.add<double>("lambda_a", 'e', "parameter a of beta prior of lambda", false, 1);
  p.add<double>("lambda_b", 'f', "parameter b of beta prior of lambda", false, 1);
  p.add<double>("lambda_c", 'g', "lambda concentration", false, 10);
  p.add<double>("alpha", 'h', "topic dirichlet prior", false, 10);
  p.add<double>("alpha0", 'i', "topic0 dirichelt piror", false, 0);
  p.add<int>("order", 'j', "ngram order", false, 3);
  p.add<int>("num_topics", 'k', "number of topics", true);
  p.add<int>("iterations", 'l', "number of iterations", false, 2000);
  p.add<int>("alpha_start", 'm', "start point of the resampling alpha", false, 1);
  p.add<int>("table_step", 'n', "table label resampling step", false, 0);
  
  p.add<int>("alpha_method", 'o', "alpha sampling method(0=uniform,1=nonuniform,2=global,3=none", false, 0);
  p.add<int>("hpylm_method", 'p', "hpylm sampling method(0=uniform,1=nonuniform,2=global,3=none", false, 0);
  p.add<int>("lambda_type", 'q', "lambda type(0=wood,1=hierarchical)", false, 1);
  p.add<int>("init", 's', "initialization method(0=random,1=sequential,2=partially)", false, 0);
  p.add<double>("prior_pass", 't', "parameter a of beta prior of stop prob", false, 1.0);
  p.add<double>("prior_stop", 'u', "parameter b of beta prior of stop prob", false, 1.0);
  
  p.add<int>("init_depth", 'v', "initial depth in random initialization", false, 2);
  p.add<int>("tree_type", 'w', "tree type(0=graphical,1=non_graphical)", false, 0);

  p.parse_check(argc, argv);
  
  int init_depth = p.get<int>("init_depth");
  if (init_depth >= p.get<int>("order")) {
    init_depth = p.get<int>("order") - 1;
  }

  double alpha0 = p.get<double>("alpha0");
  if (p.get<int>("tree_type") == 1) {
    alpha0 = 0.0;
  }

  try {
    hpy_lda::init_rnd();
    StartLogging(argv, p.get<string>("model"));

    hpy_lda::HpyLdaModel<hpy_lda::CachedHpyLdaSampler> model(
        p.get<double>("lambda_a"),
        p.get<double>("lambda_b"),
        p.get<double>("lambda_c"),
        p.get<double>("prior_pass"),
        p.get<double>("prior_stop"),
        p.get<double>("discount"),
        p.get<double>("concentration"),
        alpha0,
        p.get<double>("alpha"),
        p.get<int>("num_topics"),
        p.get<int>("order"),
        hpy_lda::LambdaType(p.get<int>("lambda_type")),
        hpy_lda::TreeType(p.get<int>("tree_type")));
    
    model.ReadTrainFile(p.get<string>("file"));
    model.SetSampler();
    model.SetAlphaSampler(hpy_lda::HyperSamplerType(p.get<int>("alpha_method")));
    model.SetHpySampler(hpy_lda::HyperSamplerType(p.get<int>("hpylm_method")));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();

    int i = 0;
    int init = p.get<int>("init");
    if (init == 0 || init == 2) {
      sampler.InitializeInRandom(init_depth, init == 0);
    } else {
      sampler.RunOneIteration(0);
    }
    int num_iterations = p.get<int>("iterations");
    for (i = 1; i < num_iterations; ++i) {
      sampler.RunOneIteration(i);
      
      if (i < 10 || (i+1) % 50 == 0) {
        model.SaveModels(p.get<string>("model"), i);
      }
    }
    if (i % 50 != 0) {
      model.SaveModels(p.get<string>("model"), i);
    }
    cerr << "\nsampling done!" << endl;
    ct_analyzer.CheckInternalConsistency();
  } catch (string what) {
    cerr << what << endl;
    return 1;
  }
}
