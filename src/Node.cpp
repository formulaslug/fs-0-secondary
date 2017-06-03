// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

#include "Node.h"

#include <cstring>

Node::Node(const char* nameStr) {
  // std::strncpy() doesn't exist on this platform, so tell the linter to ignore
  // it
  std::strcpy(name, nameStr);  // NOLINT
}

void Node::addChild(std::unique_ptr<Node> child) {
  child->parent = this;
  children.push_back(std::move(child));
}

void Node::draw(Display* displays) {}
