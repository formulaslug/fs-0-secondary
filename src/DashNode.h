// Copyright (c) Formula Slug 2016. All Rights Reserved.

#ifndef DASH_NODE_H
#define DASH_NODE_H

#include "Node.h"

class DashNode : public Node {
 public:
  explicit DashNode(const char* nameStr = "- - no name - -");

  void draw(Display* displays) override;
};

#endif  // DASH_NODE_H
