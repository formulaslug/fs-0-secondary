#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

#include "Node.h"

class SensorNode : public Node {
 public:
  SensorNode(const char* nameStr = "- - no name (s) - -");

  void draw(Display* displays) override;
};

#endif // SENSOR_NODE_H
