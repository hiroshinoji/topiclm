#include <string>
#include "cmdline.h"
#include "random_util.hpp"
#include "topiclm_model.hpp"
#include "log.hpp"
#include "particle_filter_stream_manager.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  cmdline::parser p;
  p.add<string>("file", 'f', "test file; if ommitted, read from input stream", false);
  p.add<int>("particles", 'p', "nubmer of particles", false, 1);
  p.add<int>("step", 's', "reestimate step size", false, 1);
  p.add<string>("unit", 'u', "reestimate unit (sentence|word)", false, "sentence");
  p.add<string>("model", 'm', "model file name (not directory)", true);
  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();

    auto unit = p.get<string>("unit");
    auto file = p.get<string>("file");

    auto model = topiclm::LoadModel<topiclm::HpyLdaSampler>(p.get<string>("model"));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();
    
    cerr << "tree node size: " << ct_analyzer.CountNodes() << endl;

    auto pf_dmanager = model.GetPFDocumentManager(p.get<int>("particles"));
    pf_dmanager.Read(p.get<string>("file"));
    auto pf_sampler = sampler.GetParticleFilterSampler(pf_dmanager);

    double ppl = pf_sampler.Run(p.get<int>("step"), cout);
    cerr << "PPL: " << ppl << endl;
    cout << "PPL: " << ppl << endl;
  } catch (string& what) {
    cerr << what << endl;
    return 1;
  }
  return 0;
}
