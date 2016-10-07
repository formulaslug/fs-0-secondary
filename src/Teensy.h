// Copyright (c) Formula Slug 2016. All Rights Reserved.

#ifndef TEENSY_H
#define TEENSY_H

#include <atomic>
#include <memory>

class Node;

// Determines which GUI to display: dashboard or menu
enum class DisplayState { Dash, Menu };

struct Teensy {
  explicit Teensy(std::unique_ptr<Node> headNode);

  std::atomic<DisplayState> displayState{DisplayState::Dash};
  std::unique_ptr<Node> headNode;
  Node* currentNode;
  std::atomic<bool> redrawScreen{true};  // Trigger an initial rendering
};

#endif  // TEENSY_H
