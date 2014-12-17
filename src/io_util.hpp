#ifndef _TOPICLM_IO_UTIL_HPP_
#define _TOPICLM_IO_UTIL_HPP_

#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>
#include <sstream>
#include <fstream>
#include <pficommon/data/intern.h>
#include <pficommon/data/string/utility.h>
#include "random_util.hpp"
#include "util.hpp"

namespace topiclm {

struct ReadConfig {
  std::vector<WordConverterType> conv_types;
  UnkConverterType unk_converter_type;
  UnkHandlerType unk_handler_type;
  int unprocess_with_stream;
  int unk_threshold;
  std::string unk_type;
  TrainFileFormat format;

  template <typename Archive>
  void serialize(Archive& ar) {
    if (ar.is_read) {
      std::vector<int> conv_type_ids;
      ar & conv_type_ids;
      int unk_converter_id;
      ar & unk_converter_id;
      
      conv_types.resize(conv_type_ids.size());
      for (size_t i = 0; i < conv_types.size(); ++i) {
        conv_types[i] = WordConverterType(conv_type_ids[i]);
      }
      unk_converter_type = UnkConverterType(unk_converter_id);
    } else {
      std::vector<int> conv_type_ids(conv_types.size());
      for (size_t i = 0; i < conv_types.size(); ++i) {
        conv_type_ids[i] = int(conv_types[i]);
      }
      ar & conv_type_ids;
      int unk_converter_id = int(unk_converter_type);
      ar & unk_converter_id;
      // other information such as unk_handler_type is not used for test so we discard
    }
    ar & MEMBER(unk_type);
  }

  ReadConfig() : format(kOneDoc) {} // default is one doc mode
};

class WordConverter {
 public:
  virtual ~WordConverter() {}
  virtual std::string operator()(std::string str) const = 0;

  std::string to_lower(std::string str) const {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
  }
  bool ends_with(const std::string& str, const std::string& suffix) const {
    return str.size() >= suffix.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }
};

class LowerConverter : public WordConverter { // The -> the
 public:
  std::string operator()(std::string str) const {
    return to_lower(str);
  }
};

class NumberKiller : public WordConverter { // 12,345 -> ##,###
 public:
  std::string operator()(std::string str) const {
    for (size_t i = 0; i < str.length(); ++i) {
      if (isdigit(str[i])) {
        str[i] = '#';
      }
    }
    return str;
  }
};

/**
 * used in UnkWordHandler to convert unknown token into
 */
class ToUnkConverter : public WordConverter {
 public:
  ToUnkConverter(std::string unk_type) : unk_type_(unk_type) {}
  virtual ~ToUnkConverter() {}
  
  std::string operator()(std::string ) const {
    return unk_type_;
  }
  
 private:
  const std::string unk_type_;
  
};

/**
 * extract surface features, e.g., vexing -> UNK-ing (NOTE: this is English specific)
 * This is a re-implementation of original Java snippet found in
 * http://code.google.com/p/berkeleyparser/source/browse/trunk/src/edu/berkeley/nlp/discPCFG/LexiconFeatureExtractor.java?r=11
 */
class BerkeleySignatureExtractor : public WordConverter{
 public:
  std::string operator()(std::string str) const {
    std::stringstream ss;
    ss << "UNK";
    int wlen = str.length();
    int num_caps = 0;
    bool has_digit = false;
    bool has_dash = false;
    bool has_lower = false;

    for (int i = 0; i < wlen; ++i) {
      if (isdigit(str[i])) has_digit = true;
      else if (str[i] == '-') has_dash = true;
      else if (isalpha(str[i])) {
        if (islower(str[i])) has_lower = true;
        else {
          num_caps++;
        }
      }
    }
    char ch0 = str[0];
    std::string lowered = to_lower(str);
    if (isupper(ch0) || (!isalpha(ch0) && num_caps > 0)) ss << "-CAPS";
    else if (has_lower) ss << "-LC";
    
    if (has_digit) ss << "-NUM";
    if (has_dash) ss << "-DASH";
    if (wlen >= 3 && ends_with(lowered, "s")) {
      // here length 3, so you don't miss out on ones like 80s
      char ch2 = lowered[wlen - 2];
      // not -ess suffixes or greek/latin -us, -is
      if (ch2 != 's' && ch2 != 'i' && ch2 != 'u') {
        ss << "-s";
      }
    } else if (wlen >= 5 && !has_dash && !(has_digit && num_caps > 0)) {
      // don't do for very short words;
      // Implement common discriminating suffixes
      if (ends_with(lowered, "ed")) ss << "-ed";
      else if (ends_with(lowered, "ing")) ss << "-ing";
      else if (ends_with(lowered, "ion")) ss << "-ion";
      else if (ends_with(lowered, "er")) ss << "-er";
      else if (ends_with(lowered, "est")) ss << "-est";
      else if (ends_with(lowered, "ly")) ss << "-ly";
      else if (ends_with(lowered, "ity")) ss << "-ity";
      else if (ends_with(lowered, "y")) ss << "-y";
      else if (ends_with(lowered, "al")) ss << "-al";
    }
    
    return ss.str();
  }
};

class UnkWordHandler {
 public:
  virtual ~UnkWordHandler() {}
  virtual std::vector<std::string> convert(std::vector<std::string> original) const = 0;
};

/**
 * This class do nothing for unknown words; it assumes the input is already preprocessed.
 */ 
class NoneUnkHandler : public UnkWordHandler {
 public:
  std::vector<std::string> convert(std::vector<std::string> original) const {
    return original;
  }
};

/**
 * This class requires building a vocaburary dictionary (a.k.a., intern<string>) that is obtained by Reader.getDictionary beforehand.
 * A word which is not found in the dictionary is treated as unknown token, which is processed with unk_converter, probably either ToUnkConverter or BerkeleySignatureExtractor.
 *
 * This class can also be used for preprocessing test sentences.
 */
class UnkWordHandlerWithDict : public UnkWordHandler {
 public:
  UnkWordHandlerWithDict(const pfi::data::intern<std::string>& dict,
                         const std::shared_ptr<WordConverter> converter) :
      dict_(dict), converter_(converter) {}

  std::vector<std::string> convert(std::vector<std::string> original) const {
    for (size_t i = 0; i < original.size(); ++i) {
      if (!dict_.exist_key(original[i])) {
        original[i] = (*converter_)(original[i]);
      }
    }
    return original;
  }
  
 private:
  const pfi::data::intern<std::string>& dict_;
  const std::shared_ptr<WordConverter> converter_;
  
};

/**
 * This class mimics the behavior explained in the paper in Pals and Klein (2012).
 * It uses no unknown processing for first `num_unprocess_sentences` sentences.
 * If an unknown word is found in the following sentences, that may be replaced
 * with an unknown token in 10% probability.
 * 
 * The problem of this class is it might be weak for mixed domain problem because the first vocaburary is constructed with only limited domain. I don't know how it is problematic.
 *
 * This class receives a reference of dict, but it can be empty at the constructing time.
 * It judges whether a word is unknown using the dict at that time, which would be
 * automatically growing in the other class (Reader).
 */ 
class StreamUnkWordHandler : public UnkWordHandler {
 public:
  StreamUnkWordHandler(const pfi::data::intern<std::string>& dict,
                       const std::shared_ptr<WordConverter> converter,
                       int num_unprocess_sentences = 10000) :
      dict_(dict),
      converter_(converter),
      num_unprocess_sentences_(num_unprocess_sentences),
      num_processed_(0) {}

  std::vector<std::string> convert(std::vector<std::string> original) const {
    num_processed_++;
    if (num_processed_ < num_unprocess_sentences_) return original;
    
    for (size_t i = 0; i < original.size(); ++i) {
      if (!dict_.exist_key(original[i])) {
        if (random->NextBernoille(0.1)) {
          original[i] = (*converter_)(original[i]);
        }
      }
    }
    return original;
  }
  
 private:
  const pfi::data::intern<std::string>& dict_;
  const std::shared_ptr<WordConverter> converter_;
  const int num_unprocess_sentences_;
  mutable int num_processed_;
  
};

class Reader {
 public:
  Reader(const std::vector<std::shared_ptr<WordConverter> >& converters,
         const std::shared_ptr<UnkWordHandler> unk_handler):
      converters_(converters),
      unk_handler_(unk_handler) {}

  virtual ~Reader() {};

  virtual void Reset() = 0;
  virtual std::vector<std::string> NextDocument() = 0;

  /**
   * @param unk_threshold_ a word which count is below this is treated as unk
   */ 
  void BuildDictionary(pfi::data::intern<std::string>& dict, int unk_threshold_ = 1) {

    Reset();
    
    std::unordered_map<std::string, int> word_counts;
    
    for (std::vector<std::string> doc; !(doc = NextDocument()).empty(); ) {
      for (auto& sentence : doc) {
        auto tokens = split_strip(sentence);
        tokens = convert(tokens);
        for (auto& token : tokens) {
          word_counts[token] += 1;
        }
      }
    }
    for (auto& item: word_counts) {
      if (item.second > unk_threshold_) {
        dict.key2id(item.first);
      }
    }
  }

  std::vector<std::vector<int> > ReadDocumentFromFile(
      pfi::data::intern<std::string>& dict,
      const std::string& fn) const {
    return ReadDocument(dict, ReadLinesFromFile(fn));
  }
  
  std::vector<std::string > ReadLinesFromFile(const std::string& fn) const {
    std::vector<std::string> doc;
    std::ifstream ifs(fn.c_str());
    for (std::string line; getline(ifs, line); ) {
      line = pfi::data::string::strip(line);
      if (!line.empty()) {
        doc.push_back(line);
      }
    }
    return doc;
  }

  std::vector<std::vector<std::vector<int> > > ReadDocuments(pfi::data::intern<std::string>& dict) {
    std::vector<std::vector<std::vector<int> > > documents;
    
    for (std::vector<std::string> doc; !(doc = NextDocument()).empty(); ) {
      documents.push_back(ReadDocument(dict, doc));
    }
    return documents;
  }

  std::vector<std::vector<int> > ReadDocument(
      pfi::data::intern<std::string>& dict,
      const std::vector<std::string>& doc) const {
    std::vector<std::vector<int> > document;

    for (auto& sentence : doc) {
      document.push_back(Read(dict, sentence));
    }

    return document;
  }

  std::vector<int> Read(pfi::data::intern<std::string>& dict,
                        const std::string& sentence) const {
    return ConvertToIDs(dict, ReadTokens(sentence));
  }

  std::vector<std::string> ReadTokens(const std::string& sentence) const {
    auto tokens = split_strip(sentence);
    
    tokens = convert(tokens);
    tokens = unk_handler_->convert(tokens);
    return tokens;
  }
  
  std::vector<int> ConvertToIDs(pfi::data::intern<std::string>& dict,
                                std::vector<std::string> tokens) const {
    std::vector<int> token_ids(tokens.size());
    
    for (size_t i = 0; i < tokens.size(); ++i) {
      token_ids[i] = dict.key2id(tokens[i]);
    }
    token_ids.push_back(dict.key2id(kEosKey));

    return token_ids;
  }
  
  std::vector<std::string> convert(std::vector<std::string> orig) const {
    for (size_t i = 0; i < orig.size(); ++i) {
      orig[i] = convert(orig[i]);
    }
    return orig;
  }
  
  std::string convert(std::string str) const {
    for (auto& conv: converters_) {
      str = (*conv)(str);
    }
    return str;
  }
  
 private:
  const std::vector<std::shared_ptr<WordConverter> > converters_;
  const std::shared_ptr<UnkWordHandler> unk_handler_;
  
};

/**
 * This class assumes the input file is one file where each document is segmented by an empty line.
 */
class OneDocReader : public Reader {
 public:
  OneDocReader(const std::string& fn,
               const std::vector<std::shared_ptr<WordConverter> >& converters,
               const std::shared_ptr<UnkWordHandler> unk_handler):
      Reader(converters, unk_handler), fn_(fn) {}

  virtual ~OneDocReader() {
    Reset();
  }
  
  virtual void Reset() {
    if (ifs.is_open()) ifs.close();
  }
  
  virtual std::vector<std::string> NextDocument() {
    if (!ifs.is_open()) {
      ifs.open(fn_);
    }
    if (ifs.eof()) return {};
    
    std::vector<std::string> doc;
    for (std::string line; getline(ifs, line); ) {
      line = pfi::data::string::strip(line);
      if (line.empty()) {
        if (!doc.empty()) break;
      } else {
        doc.push_back(line);
      }
    }
    
    return doc;
  }
  
 private:
  std::string fn_;
  std::ifstream ifs;
  
};

/**
 * This class assumes the input file lists absolute paths of all documents
 */ 
class MultiDocReader : public Reader {
 public:
  MultiDocReader(const std::string& fn,
                 const std::vector<std::shared_ptr<WordConverter> >& converters,
                 const std::shared_ptr<UnkWordHandler> unk_handler):
      Reader(converters, unk_handler), fn_(fn) {}
  
  virtual void Reset() {
    current_doc_ = 0;
  }
  
  virtual std::vector<std::string> NextDocument() {
    if (file_lists_.empty()) ReadFileLists();
    if (current_doc_ >= (int)file_lists_.size()) return {};

    auto doc = ReadLinesFromFile(file_lists_[current_doc_]);
    current_doc_++;
    if (doc.empty()) {
      std::cerr << "file in " << file_lists_[current_doc_]
                << " does not exist or empty, so skipped." << std::endl;
      return NextDocument();
    } else return doc;
  }
  
 private:
  void ReadFileLists() {
    std::ifstream ifs(fn_.c_str());
    for (std::string line; getline(ifs, line); ) {
      line = pfi::data::string::strip(line);
      file_lists_.push_back(line);
    }
    Reset();
    if (file_lists_.empty()) throw "No file found in " + fn_;
  }
  
  std::string fn_;
  std::vector<std::string> file_lists_;
  int current_doc_;
  
};

/**
 * Used in interactive mode
 */
class SentenceReader : public Reader {
 public:
  SentenceReader(const std::vector<std::shared_ptr<WordConverter> >& converters,
                 const std::shared_ptr<UnkWordHandler> unk_handler):
      Reader(converters, unk_handler) {}

  virtual void Reset() {}
  virtual std::vector<std::string> NextDocument() { return {}; }
  
};

} // topiclm



#endif /* _TOPICLM_IO_UTIL_HPP_ */
