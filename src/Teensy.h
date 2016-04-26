#ifndef TEENSY_H
#define TEENSY_H

#include <cstdint>

class Node;

// Determines which GUI to display: dashboard or menu
enum class DisplayState {
  Dash,
  Menu
};

struct Teensy {
  Teensy(Node* currentNode);

  DisplayState displayState = DisplayState::Dash;
  Node* currentNode;
  uint32_t menuTimer;
  bool redrawScreen = true; // Trigger an initial rendering
};

#endif // TEENSY_H
