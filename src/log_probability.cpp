#include <string>
#include <pficommon/data/string/utility.h>
#include "cmdline.h"
#include "random_util.hpp"
#include "topiclm_model.hpp"
#include "log.hpp"
#include "particle_filter_document_manager.hpp"
#include "io_util.hpp"

using namespace pfi::data::string;
using namespace std;

bool init_store(const string& mode_str) {
  if (mode_str == "store") return true;
  else if (mode_str == "readOnly") return false;
  else throw "Invalid mode option: " + mode_str;
}

void RunShell(topiclm::ParticleFilterSampler& pf_sampler,
              pfi::data::intern<std::string>& dict,
              std::shared_ptr<topiclm::Reader> reader,
              bool store,
              const bool calc_eos) {

  string query;
  
  for (;;) {
    cout << "> " << flush;
    if (!getline(cin, query)) break;
    query = strip(query);
    if (query.empty()) continue;
    
    if (query == "store:") {
      cout << "Changed the mode to store each input sentence and update the topic distribution." << endl;
      store = true;
      
    } else if (query == "readonly:") {
      cout << "Changed the mode to only calculate the probability of a given sentence and not store." << endl;
      store = false;
      
    } else if (query == "reestimate:") {
      pf_sampler.ResampleAll();
      
    } else {
      auto tokens = reader->ReadTokens(query);
      for (auto& t: tokens) cout << t << " ";
      cout << endl;

      auto token_ids = reader->Read(dict, query);
      if (!calc_eos) token_ids.pop_back();
      
      cout << "log probability: "
           << pf_sampler.log_probability(token_ids, store) << endl;
    }
  }
}

void CalcDocumentProbability(const std::string& fn,
                             topiclm::ParticleFilterSampler& pf_sampler,
                             pfi::data::intern<std::string>& dict,
                             std::shared_ptr<topiclm::Reader> reader,
                             const bool calc_eos) {
  
  auto document = reader->ReadDocumentFromFile(dict, fn);
  if (!calc_eos) {
    for (auto& sentence : document) sentence.pop_back();
  }
  double log_prob = 0;
  int num_tokens = 0;
  
  for (auto& sentence : document) {
    log_prob += pf_sampler.log_probability(sentence, true);
    num_tokens += sentence.size();
  }
  double perplexity = std::exp(-log_prob / num_tokens);
  
  cout << "document log probability: " << log_prob << endl;
  cout << "# tokens"
       << (calc_eos? " (including EOSs): " : ": ")
       << num_tokens << endl;
  cout << "perplexity: " << perplexity << endl;
}

int main(int argc, char *argv[])
{
  cmdline::parser p;
  p.add<string>("file", 'f', "input file (treated as one document); run shell-mode if omitted", false);
  p.add<int>("particles", 'p', "nubmer of particles", false, 1);
  p.add<int>("step", 's', "reestimate each after storing this number of sentences", false, 1);
  p.add<string>("mode", 'M', "initial model (store|readonly)", false, "store");
  p.add<string>("model", 'm', "model file name (not directory)", true);
  p.add<bool>("calc_eos", 'e', "Whether the sentence probability contains each EOS probability", false, true);
  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();

    bool store = init_store(p.get<string>("mode"));

    auto model = topiclm::LoadModel<topiclm::HpyLdaSampler>(p.get<string>("model"));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();
    
    cerr << "tree node size: " << ct_analyzer.CountNodes() << endl;

    auto pf_dmanager = model.GetPFDocumentManager(p.get<int>("particles"));
    pf_dmanager.Reset();
    auto pf_sampler = sampler.GetParticleFilterSampler(pf_dmanager, p.get<int>("step"));

    auto reader = model.reader_for_test();

    std::cerr << std::endl;
    if (!p.exist("file")) {
      RunShell(pf_sampler, pf_dmanager.intern(), reader, store, p.get<bool>("calc_eos"));
    } else {
      CalcDocumentProbability(
          p.get<string>("file"), pf_sampler, pf_dmanager.intern(), reader, p.get<bool>("calc_eos"));
    }
    
  } catch (string& what) {
    cerr << what << endl;
    return 1;
  }
  return 0;
}
