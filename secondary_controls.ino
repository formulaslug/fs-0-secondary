/*
 * @desc Pimary control system for the UCSC's Formula Slug Electric FSAE Vehicle
 * @dict LV = Low Voltage System, HV = High Voltage System, RTD = Ready-To-Drive
 */

// TODO: Need some button debounce

#include <cstdint>
#include <cstdio>
#include <SPI.h>

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include <font_Arial.h>
/* ILI9341.h defines a swap macro that conflicts with the C++ standard library,
 * so this undefines it to avoid problems (it should be using the standard
 * library's std::swap() anyway...)
 */
#undef swap

#include "DashNode.h"
#include "MenuNode.h"
#include "Teensy.h"

constexpr uint32_t ADC_CHANGE_TOLERANCE = 3;

// Must be multiple of 20ms (1/50s)
constexpr uint32_t SUCCESSFUL_PRESS_TIME = 100;

constexpr uint32_t NUM_BTNS = 4;
constexpr uint32_t START_BTN_PIN = 2;

void _2hzTimer();
void _50hzTimer();
int btnDebounce();

static Teensy* teensy;

int main() {
  constexpr uint32_t TFT_0_DC = 15;
  constexpr uint32_t TFT_0_CS = 10;
  constexpr uint32_t TFT_1_DC = 20;
  constexpr uint32_t TFT_1_CS = 9;
  constexpr uint32_t MENU_TIMEOUT = 5000; // in ms

  // Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
  ILI9341_t3 tft[2] = {ILI9341_t3(TFT_0_CS, TFT_0_DC),
                              ILI9341_t3(TFT_1_CS, TFT_1_DC)};
  /* ILI9341_t3 tft = ILI9341_t3(TFT_0_CS, TFT_0_DC); */

  IntervalTimer interval50hz;
  IntervalTimer interval2hz;

  Serial.begin(9600);
  uint32_t i;
  for (i = 0; i < 2; i++) {
    tft[i].begin();
    tft[i].setRotation(1);
    tft[i].fillScreen(ILI9341_BLACK);
    tft[i].setTextColor(ILI9341_YELLOW);
    /* tft[0].setTextSize(2); */
    tft[i].setFont(Arial_20);
    tft[i].setCursor(0, 4); // (x,y)
  }

  // init Teensy pins
  // -> init btn pins
  for (i = START_BTN_PIN; i < (START_BTN_PIN + NUM_BTNS); i++) {
    pinMode(i, INPUT);
  }

  // create the node tree
  auto head = new DashNode(); // dash is tree head
  head->m_nodeType = NodeType::DashHead;

  // main menu
  auto menuHead = new MenuNode();
  menuHead->m_nodeType = NodeType::MenuHead;
  head->addChild(menuHead);

  auto sensors = new MenuNode("Sensors");
  sensors->addChild(new Node("Sensor 1"));
  sensors->addChild(new Node("Sensor 2"));
  sensors->addChild(new Node("Sensor 3"));
  menuHead->addChild(sensors);

  menuHead->addChild(new MenuNode("Settings"));
  menuHead->addChild(new MenuNode("Other"));

  teensy = new Teensy(head);

  /* NODES: - must have all their attributes defined, but do not need to have
   * children
   */

  // Set the interval timers
  interval50hz.begin(_50hzTimer, 20000);
  /* interval2hz.begin(_2hzTimer, 500000); */

  while(1) {
    switch(teensy->displayState) {
      // displaying dash only
      case DisplayState::Dash:
        // check if should redraw screen
        if (teensy->redrawScreen) {
          // execute drawDash func. pointed to, to update display
          teensy->currentNode->draw(tft);
          teensy->redrawScreen = false;
        }
        // check if should display menu, this btnPress is not counted for menu navigation
        if (teensy->btnPress) {
          teensy->currentNode = teensy->currentNode->children[0]; // move to mainMenu node
          teensy->displayState = DisplayState::Menu; // transition to menu state
          teensy->redrawScreen = true;
          teensy->menuTimer = 0; // clear menu timeout timer
          teensy->btnPress = BTN_NONE; // reset btn press
        }
        break;
      // displaying members of menu node tree
      case DisplayState::Menu:
        if (teensy->redrawScreen) {
          // execute draw func. pointed to in node, to update display
          teensy->currentNode->draw(tft);
          teensy->redrawScreen = false;
        }
        if (teensy->menuTimer > MENU_TIMEOUT) {
          // return to dash state
          teensy->currentNode = teensy->currentNode->parent;
          teensy->displayState = DisplayState::Dash;
        }
        switch (teensy->btnPress) {
          case BTN_0: // up..backward through child highlighted
            if (teensy->currentNode->childIndex > 0) {
              teensy->currentNode->childIndex--;
            } else {
              teensy->currentNode->childIndex = teensy->currentNode->children.size() - 1;
            }
            teensy->redrawScreen = true;
            teensy->btnPress = BTN_NONE; // reset btn press
            break;
          case BTN_1: // right..into child
            // make sure node has children
            if (teensy->currentNode->children[teensy->currentNode->childIndex]->children.size() > 0) {
              // move to the new node
              teensy->currentNode = teensy->currentNode->children[teensy->currentNode->childIndex];
              teensy->redrawScreen = true;
            } else {
              // (should show that item has no children)
            }
            teensy->btnPress = BTN_NONE; // reset btn press
            break;
          case BTN_2: // down..forward through child highlighted
            if (teensy->currentNode->childIndex == teensy->currentNode->children.size() - 1) {
              teensy->currentNode->childIndex = 0;
            } else {
              teensy->currentNode->childIndex++;
            }
            teensy->redrawScreen = true;
            teensy->btnPress = BTN_NONE; // reset btn press
            break;
          case BTN_3: // left..out to parent
            teensy->currentNode = teensy->currentNode->parent;
            teensy->redrawScreen = true;
            if (teensy->currentNode->m_nodeType == NodeType::DashHead) {
              teensy->displayState = DisplayState::Dash;
            }
            teensy->btnPress = BTN_NONE; // reset btn press
            break;
        }
        break;
    }
  }
}

/***********************************************************************************************
* Timing-based functions
***********************************************************************************************/
void _2hzTimer() {
  static uint32_t i, newVal;
  static bool valDecreased, valIncreased;

  // @desc Check if node's observed pins changed in value and must re-render
  for (i = 0; i < teensy->currentNode->numPins; i++) {
    newVal = digitalRead(teensy->currentNode->pins[i]);
    valDecreased = ((newVal - teensy->currentNode->pinVals[i]) < -(ADC_CHANGE_TOLERANCE));
    valIncreased = ((newVal - teensy->currentNode->pinVals[i]) > (ADC_CHANGE_TOLERANCE));
    if (valDecreased || valIncreased) {
      // pin/adc val changed by more than ADC_CHANGE_TOLERANCE
      teensy->currentNode->pinVals[i] = newVal;
      teensy->redrawScreen = true;
    }
  }

  // @desc Inc. menu timeout timer if in the menu state
  if (teensy->displayState == DisplayState::Menu) {
    teensy->menuTimer += 500; // 1/2 second
  }
}

// @desc Sets button press state & performs debounce
void _50hzTimer() {
  // if there isn't a current btn press waiting to be serviced
  if (!teensy->btnPress) {
    teensy->btnPress = btnDebounce();
  }
}

/***********************************************************************************************
* Timing-based functions
***********************************************************************************************/
// @desc Button debounce, assumed to be ran in a 20hz timer
int btnDebounce() {
  static uint32_t prevState = BTN_NONE, state, timer, i;

  state = BTN_NONE;
  // set to new state if exists
  for (i = 0; i < NUM_BTNS; i++) {
    if (digitalRead(i + START_BTN_PIN) == LOW) {
      state = i + 1; // b/c btn states start at 1:BTN_0--4:BTN_3
    }
  }

  if (timer >= SUCCESSFUL_PRESS_TIME && state != BTN_NONE) {
    // btn successfully pressed
    prevState = BTN_NONE;
    timer = 0;
    return state;
  } else if (prevState != state) {
    timer = 0; // btn state changed, reset
  } else {
    timer += 20; // btn state persisted..+20ms (1/50)s
  }
  prevState = state;
  return BTN_NONE; // return none until a press is evaled
}
