#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <iostream>
#include <functional>
#include <random>
#include <climits>
#include <cassert>
#include <cmath>
#include <unordered_map>

namespace {

double fast_logsumexp(double a, double b) {
  if (a == -INFINITY && b == -INFINITY) return -INFINITY;
  if (a>b) {
    return log(1+exp(b-a)) + a;
  } else {
    return log(1+exp(a-b)) + b;
  }
}

double fast_logsumexp(std::vector<double> a) {
  if (!a.empty()) {
    double z = a[0];
    for (size_t i = 1; i < a.size(); ++i) {
      z = fast_logsumexp(z, a[i]);
    }
    return z;
  } else {
    return 1;
  }
}

}

class TemplatureManager {
 public:
  virtual ~TemplatureManager() {}
  virtual void CalcTemplature(int /*sample_idx*/) {}
  virtual double templature() { return 1.0; }
  //virtual double CalcTemplature(int sample_idx) { return 1.0; }
};

class RandomBase {
public:
  RandomBase(TemplatureManager& t_manager) : t_manager_(t_manager) {}
  virtual ~RandomBase() {}

  virtual double NextDouble() = 0;
  virtual double NextGaussian(double mean, double stddev) = 0;

  double operator()() {
    return NextDouble();
  }
  
  long int operator()(long int max) {
    return NextMult(max);
  }

  long int NextMult(long int max) {
    return NextDouble() * max;
  }

  bool NextBernoille(double trueProb) {
    if (trueProb > 1.0) return true;
    if (t_manager_.templature() != 1.0) {
      double falseProb = 1.0 - trueProb;
      double x = 1.0 / t_manager_.templature();
      trueProb = pow(trueProb, x);
      falseProb = pow(falseProb, x);
      trueProb /= (trueProb + falseProb);
    }
    return trueProb > NextDouble();
  }

  virtual double NextGamma(double a) {
    double x, y, z;
    double u, v, w, b, c, e;
    int accept = 0;
    if (a <= 1)
    {
      /* Johnk's generator. Devroye (1986) p.418 */
      e = -log(NextDouble());
      do {
        x = pow(NextDouble(), 1 / a);
        y = pow(NextDouble(), 1 / (1 - a));
      } while (x + y > 1);
      return (e * x / (x + y));
    } else {
      /* Best's rejection algorithm. Devroye (1986) p.410 */
      b = a - 1;
      c = 3 * a - 0.75;
      while(accept != 1) {
        /* generate */
        u = NextDouble();
        v = NextDouble();
        w = u * (1 - u);
        y = sqrt(c / w) * (u - 0.5);
        x = b + y;
        if (x >= 0)
        {
          z = 64 * w * w * w * v * v;
          if (z <= 1 - (2 * y * y) / x)
          {
            accept = 1;
          } else {
            if (log(z) <= 2 * (b * log(x / b) - y))
              accept = 1;
          }
        }
      }
      return x;
    }
  }

  virtual double NextGamma(double a, double b) {
    assert(a > 0);
    assert(b > 0);
    return NextGamma(a) / b;
  }

  double NextBeta(double a, double b) {
    double x = NextGamma(a), y = NextGamma(b);
    return x / (x + y);
  }
  
  template <typename ValueType>
  std::unordered_map<int, double> NextDirichlet(const std::unordered_map<int, ValueType>& alpha, double prec = 0) {
    std::unordered_map<int, double> theta;
    double z = 0;
    /* theta must have been allocated */
    for (const auto& item : alpha) {
      const int key = item.first;
      const ValueType value = item.second;
      if (prec != 0) {
        theta[key] = NextGamma(value * prec);
      } else {
        theta[key] = NextGamma(value);
      }
      if (theta[key] < 0.00000001) theta[key] = 0.00000001;
      z += theta[key];
    }
    for (auto& item : theta) {
      item.second /= z;
    }
    return theta;
  }

  template <typename ValueType>
  std::vector<double> NextDirichlet(const std::vector<ValueType>& alpha, double prec = 0) {
    std::vector<double> theta(alpha.size());
    double z = 0;
    /* theta must have been allocated */
    for (size_t i = 0; i < alpha.size(); i++) {
      if (alpha[i] != 0) {
        if (prec != 0) {
          theta[i] = NextGamma(alpha[i] * prec);
        } else {
          theta[i] = NextGamma(alpha[i]);
        }
        z += theta[i];
      }
    }
    for (size_t i = 0; i < alpha.size(); i++) {
      if (alpha[i] != 0) {
        theta[i] /= z;
      }
    }
    return theta;
  }

  template <typename T>
  int SampleUnnormalizedPdf(std::vector<T> pdf, int endPos = -1) {
    return SampleUnnormalizedPdfRef(pdf, endPos);
  }
  // 要素数が平均的に少ないので、線形の方が効率が良いのかも
  template <typename T>
  int SampleUnnormalizedPdfRef(std::vector<T>& pdf, int endPos = -1) {
    assert(pdf.size() > 0);
    // if endPos == 0, use entire vector
    if (endPos == -1) {
      endPos = pdf.size()-1;
    }
    if (endPos == 0) return 0;
    if (t_manager_.templature() != 1.0) {
      double s = 0;
      for (int i = 0; i <= endPos; ++i) {
        s += pdf[i];
      }
      double x = 1.0 / t_manager_.templature();
      for (int i = 0; i <= endPos; ++i) {
        pdf[i] = pow((pdf[i] / s), x);
      }
    }
    // compute CDF (inplace)
    for (int i = 0; i < endPos; ++i) {
      pdf[i+1] += pdf[i];
    }
    if (!(pdf[endPos] > 0)) {
      throw "pdf[endPos] <= 0";
    }
    //assert(pdf[endPos] > 0);

    // sample pos ~ Uniform(0,Z)
    double z = NextDouble() * pdf[endPos];
    //assert((z >= 0) && (z <= pdf[endPos]));
    int x = std::lower_bound(pdf.begin(), pdf.begin() + endPos + 1, z) - pdf.begin();
    return x;    
  }
  template <typename T>
  int draw_discrete(const std::vector<T>& pdf, int endPos = -1) {
    int i;
    double r, s;
    if (endPos == -1) {
      endPos = pdf.size() - 1;
    }
    for (i = 0, s = 0; i <= endPos; ++i) {
      s += pdf[i];
    }
    r = NextDouble() * s;
    for (i = 0, s = 0; i <= endPos; ++i) {
      s += pdf[i];
      if (r <= s) {
        return i;
      }
    }
    return endPos;
  }
  
  // sample from vector of log probabilities
  int SampleLogPdf(std::vector<double> lpdf) {
    double logSum = fast_logsumexp(lpdf);

    for (size_t i = 0; i < lpdf.size(); ++i) {
      lpdf[i] = exp(lpdf[i] - logSum);
    }
    return SampleUnnormalizedPdf(lpdf);
  }
  
  
  template <typename KeyType, typename ValueType>
  KeyType SampleUnnormalizedPdf(const std::unordered_map<KeyType, ValueType>& pdf) {
    double r;
    double s = 0;
    for (const auto& atom : pdf) {
      s += atom.second;
    }
    r = NextDouble() * s;
    s = 0;
    KeyType last = (*pdf.begin()).first;
    for (const auto& atom : pdf) {
      s += atom.second;
      last = atom.first;
      if (r <= s) {
        return last;
      }
    }
    return last;
  }
 protected:
  TemplatureManager& t_manager_;
};

class RandomMT : public RandomBase {
public:
  RandomMT(int seed, TemplatureManager& t_manager)
      : /*gen(std::bind(std::uniform_real_distribution<double>(0.0, 1.0),
          std::mt19937(seed))),*/
      RandomBase(t_manager), gen_(seed), uniform_(0.0, 1.0) {
  }
  virtual double NextDouble() {
    return uniform_(gen_);
  }
  virtual double NextGaussian(double mean, double stddev) {
    std::normal_distribution<> d(mean, stddev);
    return d(gen_);
  }
private:
  //std::function<double(void)> gen;
  //std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_real_distribution<double> uniform_;
};

class RandomRand : public RandomBase {
public:
  RandomRand(int seed, TemplatureManager& t_manager)
      : RandomBase(t_manager), gen(rand) { srand(seed); }
  virtual double NextDouble() {
    return static_cast<double>(gen()) / static_cast<double>(RAND_MAX);
  }
  virtual double NextGaussian(double, double) {
    throw std::string("RandomRand does not support NextGaussian()!!!");
  }
private:
  std::function<double(void)> gen;
};

#endif /* _RANDOM_H_ */
