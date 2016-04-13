#ifndef TEENSY_H
#define TEENSY_H

#include <cstdint>

class Node;

// Determines which GUI to display: dashboard or menu
enum class DisplayState {
  Dash,
  Menu
};

enum ButtonStates {
  BTN_NONE = 0,
  BTN_0, // =1, up
  BTN_1, // =2, right
  BTN_2, // =3, down
  BTN_3  // =4, left
};

struct Teensy {
  Teensy(Node* currentNode);

  DisplayState displayState = DisplayState::Dash;
  Node* currentNode;
  uint32_t menuTimer;
  bool redrawScreen = true; // Trigger an initial rendering
  int btnPress = BTN_NONE; // Carries current unserviced state of buttons
};

#endif // TEENSY_H
