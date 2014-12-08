#ifndef _LOG_H_
#define _LOG_H_

#include <iostream>
#include <cstdarg>
#include <string>
#include <fstream>
#include <unordered_map>
#include <ctime>
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

struct OFSDeleter {
  void operator()(std::ofstream* ofs) {
    time_t timer;
    time(&timer);
    *ofs << "--------" << std::endl;
    *ofs << "finish : " << ctime(&timer);
    ofs->close();
  }
};

class Logger {
public:
  void SetModel(const std::string& model) {
    prefix = model + "/log";
  }
  std::ostream& GetOfs(const std::string& logName) {
    auto ofs_it = ofss.find(logName);
    if (ofs_it == ofss.end()) {
      std::unique_ptr<std::ofstream, OFSDeleter> new_ofs(new std::ofstream());
      ofss.insert(make_pair(logName, move(new_ofs)));
      std::ofstream& ofs(*ofss[logName]);
      std::string fileName = GetNewFileName(logName);
      ofs.open(fileName);
      if (!ofs) {
        return std::cerr;
        //throw std::string("cannot open file " + fileName);
      }
      return ofs;
    } else {
      return *(*ofs_it).second;
    }
  }
private:
  std::string GetNewFileName(const std::string& logName) {
    if (prefix.empty()) prefix = ".";
    return prefix + "/" + logName + ".log";
  }
  std::string prefix;
  std::unordered_map<std::string, std::unique_ptr<std::ofstream, OFSDeleter>> ofss;
};

enum AvoidODR {
  AVOID_ODR
};

template<typename AvoidODR> struct LoggerTmp {
  static Logger logger;
};
  
template<typename AvoidODR> Logger LoggerTmp<AvoidODR>::logger;
  
typedef LoggerTmp<AvoidODR> LoggerSingle;

inline std::ostream& LOG(std::string logName) {
  std::ostream& ofs(LoggerSingle::logger.GetOfs(logName));
  return ofs;
}

inline void PrepareOutput(const std::string& modelName) {
  mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR |
    S_IRGRP | S_IXGRP |
    S_IROTH | S_IXOTH;
  if (opendir(modelName.c_str()) != nullptr) {
    throw std::string("model directory " + modelName + "already exists.");
  }
  if (mkdir(modelName.c_str(), mode) != 0 ||
      mkdir(std::string(modelName + "/model").c_str(), mode) != 0) {
    throw std::string("cannot create model directory " + modelName + ".");
  }
  if (mkdir(std::string(modelName + "/topic").c_str(), mode) != 0) {
    throw std::string("cannot create topic directory " + modelName + ".");
  }
  if (
      mkdir(std::string(modelName + "/log").c_str(), mode) != 0) {
    throw std::string("cannot create log directory " + modelName + "/log.");
  }
}

inline void StartLogging(char *argv[], const std::string& modelName) {
  std::cerr << "model: " << modelName << std::endl;
  PrepareOutput(modelName);
  
  char hostname[50];
  time_t timer;
  LoggerSingle::logger.SetModel(modelName);

  time(&timer);
  gethostname(hostname, 50);

  LOG("info") << "init   :" << std::flush;
  char *s = *argv++;
  while (s) {
    LOG("info") << " " << s;
    s = *argv++;
  }
  LOG("info") << std::endl;
  LOG("info") << "host   :" << hostname << std::endl;
  LOG("info") << "start  :" << ctime(&timer);
  LOG("info") << "--------" << std::endl;
}

#endif /* _LOG_H_ */
