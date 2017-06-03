// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

#include "Teensy.h"

#include "Node.h"

Teensy::Teensy(std::unique_ptr<Node> headNode) {
  this->headNode = std::move(headNode);
}
