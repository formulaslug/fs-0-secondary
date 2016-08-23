// Copyright (c) Formula Slug 2016. All Rights Reserved.

#include "Node.h"

#include <cstring>

Node::Node(const char* nameStr) {
  std::strncpy(name, nameStr);  // NOLINT
}

void Node::addChild(Node* child) {
  child->parent = this;
  children.push_back(child);
}

void Node::draw(Display* displays) {}
