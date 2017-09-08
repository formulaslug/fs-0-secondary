// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

#pragma once

#include "Node.h"

class DashNode : public Node {
 public:
  explicit DashNode(const char* nameStr = "- - no name - -");

  void draw(Display* displays) override;
};
