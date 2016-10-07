// Copyright (c) Formula Slug 2016. All Rights Reserved.

#include "Teensy.h"

#include "Node.h"

Teensy::Teensy(std::unique_ptr<Node> headNode) {
  this->headNode = std::move(headNode);
}
