#ifndef _HPY_LDA_CONFIG_H_
#define _HPY_LDA_CONFIG_H_

#include <vector>
#include <unordered_map>
#include <memory>
#include <boost/container/flat_map.hpp>

namespace hpy_lda {

static const int kGlobalFloorId = 0; // never change
typedef std::vector<int>::iterator witerator;

class Node;
typedef std::unique_ptr<Node> NodePtr;
typedef boost::container::flat_map<int, NodePtr> ChildMapType;
//typedef std::unordered_map<int, NodePtr> ChildMapType;

typedef int16_t topic_t;

static const std::string kEosKey = "EOS";
static const std::string kUnkKey = "__unk__";

enum LambdaType { kWood = 0, kHierarchical, kDocumentHierarchical, kDirect, kSharing };
enum TreeType { kGraphical, kNonGraphical };

}

#endif /* _HPY_LDA_CONFIG_H_ */
