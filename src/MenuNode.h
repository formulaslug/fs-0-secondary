// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

#ifndef MENU_NODE_H
#define MENU_NODE_H

#include "Node.h"

class MenuNode : public Node {
 public:
  explicit MenuNode(const char* nameStr = "- - no name - -");

  void draw(Display* displays) override;
};

#endif  // MENU_NODE_H
