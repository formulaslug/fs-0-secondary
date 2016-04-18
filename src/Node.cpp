#include "Node.h"

#include <cstring>

Node::Node(const char* nameStr) {
  std::strcpy(name, nameStr);
}

void Node::addChild(Node* child) {
  child->parent = this;
  children.push_back(child);
}

void Node::draw(Display* displays) {}
