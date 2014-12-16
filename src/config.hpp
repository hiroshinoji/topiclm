#ifndef _TOPICLM_CONFIG_H_
#define _TOPICLM_CONFIG_H_

#include <vector>
#include <unordered_map>
#include <memory>
#include <boost/container/flat_map.hpp>

namespace topiclm {

static const int kGlobalFloorId = 0; // never change
typedef std::vector<int>::iterator witerator;

class Node;
typedef std::unique_ptr<Node> NodePtr;
typedef boost::container::flat_map<int, NodePtr> ChildMapType;
//typedef std::unordered_map<int, NodePtr> ChildMapType;

typedef int16_t topic_t;

static const std::string kEosKey = "__eos__";
// static std::string kUnkKey = "__unk__";

enum LambdaType { kWood = 0, kHierarchical, kDocumentHierarchical, kDirect, kSharing };
enum TreeType { kGraphical, kNonGraphical };

enum WordConverterType { kLower = 0, kNumber = 1 };
enum UnkConverterType { kNormal = 0, kBerkeley = 1 };
enum UnkHandlerType { kNone = 0, kDict = 1, kStream = 2 };

enum TrainFileFormat { kOneDoc = 0, kFileList = 1 };

}

#endif /* _TOPICLM_CONFIG_H_ */
