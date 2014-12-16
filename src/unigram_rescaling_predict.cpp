#include <string>
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
  p.add<int>("step", 's', "reestimate step size", false, 10);
  p.add<int>("num_particles", 'p', "number of particles", false, 10);
  p.add<double>("rescaling", 'r', "rescaling factor", false, 0.7);
  p.add<string>("model", 'm', "model file name (not directory)", true);
  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();
    
    auto model = topiclm::LoadModel<topiclm::UnigramRescalingSampler>(p.get<string>("model"));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();
    
    cerr << "tree node size: " << ct_analyzer.CountNodes() << endl;

    auto pf_dmanager = model.GetPFDocumentManager(p.get<int>("num_particles"));
    pf_dmanager.Read(model.reader_for_test(p.get<string>("file")));

    double ppl = sampler.PredictInParticleFilter(pf_dmanager,
                                                 p.get<int>("num_particles"),
                                                 p.get<int>("step"),
                                                 p.get<double>("rescaling"),
                                                 cout);
    cerr << "perplexity: " << ppl << endl;
    cout << "perplexity: " << ppl << endl;
  } catch (string& what) {
    cerr << what << endl;
    return 1;
  }
  return 0;
}
