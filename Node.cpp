#include "Node.h"

#include <cstring>

Node::Node(void (*drawFunc)(Node* node), const char* nameStr) {
  this->drawFunc = drawFunc;
  std::strcpy(name, nameStr);
}

void Node::addChild(Node* child) {
  child->parent = this;
  children.push_back(child);
}
