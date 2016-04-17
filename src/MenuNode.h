#ifndef MENU_NODE_H
#define MENU_NODE_H

#include "Node.h"

class MenuNode : public Node {
 public:
  MenuNode(const char* nameStr = "- - no name - -");

  void draw(Display* displays) override;
};

#endif // MENU_NODE_H
