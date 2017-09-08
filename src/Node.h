// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "libs/ILI9341_t3.h"

/* ILI9341.h defines a swap macro that conflicts with the C++ standard library,
 * so this undefines it to avoid problems (it should be using the standard
 * library's std::swap() anyway...)
 */
#undef swap

typedef ILI9341_t3 Display;

enum class NodeType {
  DashHead,
  MenuHead,
  None,
};

class Node {
 public:
  explicit Node(const char* nameStr);

  void addChild(std::unique_ptr<Node> child);
  virtual void draw(Display* displays);

  static constexpr uint32_t k_maxNumPins = 10;
  static constexpr uint32_t k_maxNodeNameChars = 20;

  char name[k_maxNodeNameChars + 1] = {};
  NodeType m_nodeType = NodeType::None;
  void (*drawFunc)(Node* node) = nullptr;  // draw function pointer

 public:
  std::vector<std::unique_ptr<Node>> children;
  uint32_t childIndex = 0;
  Node* parent = nullptr;
  uint32_t pins[k_maxNumPins] = {};
  uint32_t pinVals[k_maxNumPins] = {};
  uint32_t numPins = 0;
};
