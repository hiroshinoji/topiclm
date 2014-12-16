#include <string>
#include <memory>
#include "cmdline.h"
#include "random_util.hpp"
#include "topiclm_model.hpp"
#include "log.hpp"
#include "particle_filter_document_manager.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  cmdline::parser p;
  p.add<string>("file", 'f', "test file", true);
  p.add<int>("particles", 'p', "nubmer of particles", false, 1);
  p.add<int>("step", 's', "reestimate step size", false, 1);
  p.add<string>("model", 'm', "model file name (not directory)", true);
  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();
    
    auto model = topiclm::LoadModel<topiclm::HpyLdaSampler>(p.get<string>("model"));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();
    
    cerr << "tree node size: " << ct_analyzer.CountNodes() << endl;

    auto pf_dmanager = model.GetPFDocumentManager(p.get<int>("particles"));
    
    pf_dmanager.Read(model.reader_for_test(p.get<string>("file")));
    auto pf_sampler = sampler.GetParticleFilterSampler(pf_dmanager, p.get<int>("step"));

    double ppl = pf_sampler.Run(cout);
    cerr << "perplexity: " << ppl << endl;
    cout << "perplexity: " << ppl << endl;
  } catch (string& what) {
    cerr << what << endl;
    return 1;
  }
  return 0;
}
