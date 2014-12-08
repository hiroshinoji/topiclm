#include <iostream>
#include <vector>
#include "random.hpp"

using namespace std;

int main(int argc, char *argv[])
{
  RandomMT random;
  vector<double> dist = {1,2,3,4,5};

  cout << random.SampleUnnormalizedPdf(move(dist), 3) << endl;

  

  // for (int i = 0; i < 2; ++i) {
  //   dist[i] = i;
  // }
  //for (auto p : dist) cout << p << endl;
  
  return 0;
}
