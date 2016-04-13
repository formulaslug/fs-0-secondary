#ifndef DASH_NODE_H
#define DASH_NODE_H

#include "Node.h"

class DashNode : public Node {
 public:
  DashNode(const char* nameStr = "- - no name - -");

  void draw(Display* displays) override;
};

#endif // DASH_NODE_H
