#include <string>
#include <pficommon/data/string/utility.h>
#include "cmdline.h"
#include "random_util.hpp"
#include "topiclm_model.hpp"
#include "log.hpp"
#include "particle_filter_document_manager.hpp"

using namespace pfi::data::string;
using namespace std;

void AnalyzeRestaurant(topiclm::ContextTreeAnalyzer& ct_analyzer) {
  string query;
  for (;;) {
    putchar('>');
    getline(cin, query);
    if (query.size() == 0) return;

    vector<string> ngram = split(query, ' ');
    ct_analyzer.AnalyzeRestaurant(ngram, cout);
  }
}

void AnalyzeTopic(topiclm::ContextTreeAnalyzer& ct_analyzer) {
  string query;
  for (;;) {
    putchar('>');
    getline(cin, query);
    if (query.size() == 0) break;

    vector<string> ngram = split(query, ' ');
    ct_analyzer.AnalyzeTopic(ngram, cout);
  }
}
void AnalyzeLambda(topiclm::ContextTreeAnalyzer& ct_analyzer) {
  ct_analyzer.AnalyzeLambdaToNgrams(cout);
}
void AnalyzeTopicalNgram(topiclm::ContextTreeAnalyzer& ct_analyzer, int K) {
  ct_analyzer.CalcTopicalNgrams(K, cout);
}
void AnalyzeRelativeNgram(topiclm::ContextTreeAnalyzer& ct_analyzer, int K) {
  ct_analyzer.CalcRelativeTopicalNgrams(K, cout);
}
void CheckModel(topiclm::ContextTreeAnalyzer& ct_analyzer) {
  ct_analyzer.CheckInternalConsistency();
}

int main(int argc, char *argv[])
{
  cmdline::parser p;
  p.add<string>("model", 'm', "model file name (not directory)", true);
  p.add<int>("K", 'k', "num of displays (effective only in topical_ngram/relative_ngram", false, 10);

  p.add("restaurant");
  p.add("topic");
  p.add("lambda");
  p.add("topical_ngram");
  p.add("relative_ngram");
  p.add("check");
  
  p.parse_check(argc, argv);

  try {
    topiclm::init_rnd();
    
    auto model = topiclm::LoadModel<topiclm::HpyLdaSampler>(p.get<string>("model"));

    auto& sampler = model.sampler();
    auto ct_analyzer = sampler.GetCTAnalyzer();
    
    cerr << "tree node size: " << ct_analyzer.CountNodes() << endl;

    if (p.exist("restaurant")) {
      cerr << "analyze restaurant: " << endl;
      AnalyzeRestaurant(ct_analyzer);
    } else if (p.exist("topic")) {
      cerr << "analyze topic: " << endl;
      AnalyzeTopic(ct_analyzer);
    } else if (p.exist("lambda")) {
      cerr << "analyze lambda: " << endl;
      AnalyzeLambda(ct_analyzer);
    } else if (p.exist("topical_ngram")) {
      cerr << "analyze topical ngrams: " << endl;
      AnalyzeTopicalNgram(ct_analyzer, p.get<int>("K"));
    } else if (p.exist("relative_ngram")) {
      cerr << "analyze relative topica ngrams: " << endl;
      AnalyzeRelativeNgram(ct_analyzer, p.get<int>("K"));
    } else if (p.exist("check")) {
      CheckModel(ct_analyzer);
    } else {
      cerr << "please select mode" << endl;
      return 1;
    }
  } catch (string& what) {
    cerr << what << endl;
    return 1;
  }
  return 0;
}
