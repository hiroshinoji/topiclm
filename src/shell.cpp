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

void Run(topiclm::ParticleFilterSampler& pf_sampler,
         pfi::data::intern<std::string>& dict,
         std::shared_ptr<topiclm::Reader> reader,
         bool store) {

  std::cerr << std::endl;
  
  string query;
  
  for (;;) {
    putchar('>');
    getline(cin, query);
    if (query.size() == 0) break;
    
    query = strip(query);
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
      cout << "log probability: "
           << pf_sampler.log_probability(reader->Read(dict, query), store) << endl;
    }
  }
}

int main(int argc, char *argv[])
{
  cmdline::parser p;
  p.add<int>("particles", 'p', "nubmer of particles", false, 1);
  p.add<int>("step", 's', "reestimate each after storing this number of sentences", false, 1);
  p.add<string>("mode", 'M', "initial model (store|readonly)", false, "store");
  p.add<string>("model", 'm', "model file name (not directory)", true);
  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();

    bool store = init_store(p.get<string>("mode"));

    auto model = topiclm::LoadModel<topiclm::HpyLdaSampler>(p.get<string>("model"));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();
    
    cerr << "tree node size: " << ct_analyzer.CountNodes() << endl;

    auto reader = model.reader_for_test();

    auto pf_dmanager = model.GetPFDocumentManager(p.get<int>("particles"));
    pf_dmanager.Reset();
    auto pf_sampler = sampler.GetParticleFilterSampler(pf_dmanager, p.get<int>("step"));

    Run(pf_sampler, pf_dmanager.intern(), reader, store);
    
  } catch (string& what) {
    cerr << what << endl;
    return 1;
  }
  return 0;
}
