#ifndef TEENSY_H
#define TEENSY_H

#include <atomic>

class Node;

// Determines which GUI to display: dashboard or menu
enum class DisplayState {
  Dash,
  Menu,
  Sensor
};

struct Teensy {
  Teensy(Node* currentNode);

  std::atomic<DisplayState> displayState{DisplayState::Dash};
  Node* currentNode;
  std::atomic<bool> redrawScreen{true}; // Trigger an initial rendering
  uint16_t inputThrottleVoltage;
};

#endif // TEENSY_H
