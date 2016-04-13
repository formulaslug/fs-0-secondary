#include "Node.h"

#include <cstring>

Node::Node(const char* nameStr) {
  std::strcpy(name, nameStr);
}
