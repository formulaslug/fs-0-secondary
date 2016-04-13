#ifndef NODE_H
#define NODE_H

#include <cstdint>
#include <vector>

enum class NodeType {
  DashHead,
  MenuHead,
  None,
};

class Node {
 public:
  Node(void (*drawFunc)(Node* node), const char* nameStr = "- - no name - -");

  void addChild(Node* child);

  static constexpr uint32_t k_maxNumPins = 10;
  static constexpr uint32_t k_maxNodeNameChars = 20;

  char name[k_maxNodeNameChars + 1] = {};
  NodeType m_nodeType = NodeType::None;
  void (*drawFunc) (Node* node) = nullptr; // draw function pointer
 public:
  std::vector<Node*> children;
  uint32_t childIndex = 0;
  Node* parent = nullptr;
  uint32_t pins[k_maxNumPins] = {};
  uint32_t pinVals[k_maxNumPins] = {};
  uint32_t numPins = 0;
};

#endif // NODE_H
