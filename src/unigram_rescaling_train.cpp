#include <string>
#include "cmdline.h"
#include "random_util.hpp"
#include "topiclm_model.hpp"
#include "log.hpp"
#include "sampling_configuration.hpp"

using namespace std;

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

  p.add<string>("word_converters", 'C', "list of word converters to apply for each word (ex: -c \"0 1\") (0=lower casing all words; 1=replace all number charactors to # (ex: 12,345=>##,###))", false, "");
  p.add<int>("unk_converter", 'u', "How to convert an unknown token? (0=replace with unk_type; 1=replace with a signature of a surface (e.g., vexing -> UNK-ing; NOTE: English spcific))", false, 0);
  p.add<string>("unk_type", 'T', "a type assigned for unknown token (used only when unk_converter=0)", false, "__unk__");
  p.add<int>("unk_handler", 'H', "When a token is recognized as an unknown token? (0=do nothing; 1=all tokens which counts are below unk_threshold; 2=nothing for first `unk_stream_start` sentences, then, 10% of new words are recognized as unknown)", false, 1);
  
  p.add<int>("unk_stream_start", 'S', "When `unk_handler`=2, some new words in sentences which is after this number of sentences are treated as unknown", false, 10000);
  p.add<int>("unk_threshold", 'U', "(used only when `unk_converter`=0)", false, 1);

  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();
    StartLogging(argv, p.get<string>("model"));

    topiclm::HpyLdaModel<topiclm::UnigramRescalingSampler> model(
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
        topiclm::LambdaType(0),
        topiclm::kGraphical,
        read_config(p));

    std::cerr << "\ntraining model setting:" << std::endl;
    std::cerr << "--------------------" << std::endl;
    std::cerr << model.status();
    std::cerr << "--------------------\n" << std::endl;
    
    model.ReadTrainFile(p.get<string>("file"));
    model.SetSampler();
    model.SetAlphaSampler(topiclm::HyperSamplerType(p.get<int>("alpha_method")));
    model.SetHpySampler(topiclm::HyperSamplerType(p.get<int>("hpylm_method")));

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
