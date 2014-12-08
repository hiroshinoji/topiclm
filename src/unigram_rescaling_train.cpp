#include <string>
#include "cmdline.h"
#include "random_util.hpp"
#include "hpy_lda_model.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"

using namespace std;

int main(int argc, char** argv) {
  cmdline::parser p;
  p.add<string>("file", 'f', "training file", true);
  p.add<string>("model", 'm', "directory name where output/log will be stored", true);
  p.add<double>("concentration", 'c', "initial concentration", false, 0.1);
  p.add<double>("discount", 'd', "initial discount", false, 0.5);
  p.add<double>("alpha", 'h', "topic dirichlet prior", false, 10);
  p.add<double>("beta", 'B', "word dirichlet prior", false, 0.1);
  p.add<int>("order", 'O', "ngram order", false, 3);
  p.add<int>("num_topics", 'K', "number of topics", true);
  p.add<int>("burn-ins", 'b', "number of iterations", false, 500);
  p.add<int>("num-samples", 'n', "number of samples after burn-in", false, 20);
  p.add<int>("interval", 'i', "number of iterations in each interval of samples after burn-in", false, 20);
  p.add<int>("alpha_method", 'x', "alpha sampling method(0=uniform,1=nonuniform,2=global,3=none", false, 0);
  p.add<int>("hpylm_method", 'p', "hpylm sampling method(0=uniform,1=nonuniform,2=global,3=none", false, 0);
  p.add<int>("init", 's', "initialization method(0=random,1=sequential)", false, 0);
  

  p.parse_check(argc, argv);

  try {
    hpy_lda::init_rnd();
    StartLogging(argv, p.get<string>("model"));

    hpy_lda::HpyLdaModel<hpy_lda::UnigramRescalingSampler> model(
        1,
        0,
        1,
        1.0,
        1.0,
        p.get<double>("discount"),
        p.get<double>("concentration"),
        p.get<double>("alpha"),
        p.get<double>("alpha"),
        p.get<int>("num_topics"),
        p.get<int>("order"),
        hpy_lda::LambdaType(0),
        hpy_lda::kGraphical);
    
    model.ReadTrainFile(p.get<string>("file"));
    model.SetSampler();
    model.SetAlphaSampler(hpy_lda::HyperSamplerType(p.get<int>("alpha_method")));
    model.SetHpySampler(hpy_lda::HyperSamplerType(p.get<int>("hpylm_method")));

    auto& sampler = model.sampler();
    sampler.SetBeta(p.get<double>("beta"));
    auto ct_analyzer = sampler.GetCTAnalyzer();

    int num_burnins = p.get<int>("burn-ins");
    int interval = p.get<int>("interval");
    int num_samples = num_burnins + p.get<int>("num-samples") * interval;
    
    int i = 0;
    if (p.get<int>("init") == 0) {
      sampler.InitializeInRandom();
    } else {
      sampler.RunOneIteration(0);
    }
    
    for (i = 1; i < num_samples; ++i) {
      sampler.RunOneIteration(i);
      if (i >= num_burnins && (i - num_burnins) % interval == 0) {
        model.SaveModels(p.get<string>("model"), i);
      }
    }
    cerr << "\nsampling done!" << endl;
    ct_analyzer.CheckInternalConsistency();
  } catch (string what) {
    cerr << what << endl;
    return 1;
  }
}
