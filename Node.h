#ifndef NODE_H
#define NODE_H

#include <cstdint>
#include <vector>

enum class NodeName {
  DashHead,
  MenuHead,
  None,
};

class Node {
 public:
  Node(const char* nameStr = "- - no name - -");

  static constexpr uint32_t k_maxNumPins = 10;
  static constexpr uint32_t k_maxNodeNameChars = 20;

  NodeName m_nodeName = NodeName::None;
  char name[k_maxNodeNameChars + 1] = {};
  void (*draw) (Node* node) = nullptr; // draw function pointer
  std::vector<Node*> children;
  uint32_t currentChildIndex = 0;
  Node* parent = nullptr;
  uint32_t pins[k_maxNumPins] = {};
  uint32_t pinVals[k_maxNumPins] = {};
  uint32_t numPins = 0;
};

#endif // NODE_H
