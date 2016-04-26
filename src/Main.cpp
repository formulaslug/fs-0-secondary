/*
 * @desc Pimary control system for the UCSC's Formula Slug Electric FSAE Vehicle
 * @dict LV = Low Voltage System, HV = High Voltage System, RTD = Ready-To-Drive
 */

// TODO: Need some button debounce

#include <cstdint>
#include <atomic>
#include <IntervalTimer.h>

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include "core_controls/CANopen.h"
#include "libs/font_Arial.h"
#include "libs/ILI9341_t3.h"
#include "ButtonTracker.h"
#include "DashNode.h"
#include "MenuNode.h"
#include "Teensy.h"

enum ButtonStates {
  BTN_NONE = 0x0,
  BTN_UP = 0x1,
  BTN_RIGHT = 0x2,
  BTN_DOWN = 0x4,
  BTN_LEFT = 0x8
};

constexpr uint32_t ADC_CHANGE_TOLERANCE = 3;

// Must be multiple of 20ms (1/50s)
constexpr uint32_t SUCCESSFUL_PRESS_TIME = 100;

// Number of buttons
constexpr uint32_t k_numBtns = 4;

// First pin used by buttons. The rest follow in sequentially increasing order.
constexpr uint32_t k_startBtnPin = 5;

void _2hzTimer();
void _50hzTimer();
void btnDebounce();

static Teensy* gTeensy;
static CANopen* gCanBus = nullptr;

static CAN_message_t gTxMsg;
static CAN_message_t gRxMsg;

static ButtonTracker<4> upButton(k_startBtnPin, false);
static ButtonTracker<4> rightButton(k_startBtnPin + 1, false);
static ButtonTracker<4> downButton(k_startBtnPin + 2, false);
static ButtonTracker<4> leftButton(k_startBtnPin + 3, false);
static std::atomic<uint8_t> gBtnPressEvents{0};
static std::atomic<uint8_t> gBtnReleaseEvents{0};
static std::atomic<uint8_t> gBtnHeldEvents{0};

void canTxISR() {
  gTxMsg.len = 8;
  gTxMsg.id = 0x222;
  for (uint32_t i = 0; i < 8; i++) {
    gTxMsg.buf[i] = '0' + i;
  }

  for (uint32_t i = 0; i < 6; i++) {
    if (!gCanBus->sendMessage(gTxMsg)) {
      // Serial.println("tx failed");
    }
    gTxMsg.buf[0]++;
  }
}

void canRxISR() {
  while (gCanBus->recvMessage(gRxMsg)) {
  }
}

int main() {
  constexpr uint32_t k_ID = 0x680;
  constexpr uint32_t k_baudRate = 500000;
  gCanBus = new CANopen(k_ID, k_baudRate);

  IntervalTimer canTxInterrupt;
  canTxInterrupt.begin(canTxISR, 100000);

  IntervalTimer canRxInterrupt;
  canRxInterrupt.begin(canRxISR, 3000);

  constexpr uint32_t TFT_0_DC = 15;
  constexpr uint32_t TFT_0_CS = 10;
  constexpr uint32_t TFT_1_DC = 20;
  constexpr uint32_t TFT_1_CS = 9;
  constexpr uint32_t MENU_TIMEOUT = 5000; // in ms

  // Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
  ILI9341_t3 tft[2] = {ILI9341_t3(TFT_0_CS, TFT_0_DC, 255, 11, 14),
                       ILI9341_t3(TFT_1_CS, TFT_1_DC, 255 ,11, 14)};

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
  for (i = k_startBtnPin; i < (k_startBtnPin + k_numBtns); i++) {
    pinMode(i, INPUT_PULLUP);
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

  gTeensy = new Teensy(head);

  /* NODES: - must have all their attributes defined, but do not need to have
   * children
   */

  // Set the interval timers
  interval50hz.begin(_50hzTimer, 20000);
  // interval2hz.begin(_2hzTimer, 500000);

  while (1) {
    switch (gTeensy->displayState) {
      // displaying dash only
      case DisplayState::Dash:
        // check if should redraw screen
        if (gTeensy->redrawScreen) {
          // execute drawDash func. pointed to, to update display
          gTeensy->currentNode->draw(tft);
          gTeensy->redrawScreen = false;
        }

        // check if should display menu, this btnPress is not counted for menu navigation
        if (gBtnPressEvents != BTN_NONE) {
          Serial.println("Dash !NONE");

          gTeensy->currentNode = gTeensy->currentNode->children[0]; // move to mainMenu node
          gTeensy->displayState = DisplayState::Menu; // transition to menu state
          gTeensy->redrawScreen = true;
          gTeensy->menuTimer = 0; // clear menu timeout timer

          // Consume all button events
          gBtnPressEvents = BTN_NONE;
        }
        break;
      // displaying members of menu node tree
      case DisplayState::Menu:
        if (gTeensy->redrawScreen) {
          // execute draw func. pointed to in node, to update display
          gTeensy->currentNode->draw(tft);
          gTeensy->redrawScreen = false;
        }

        if (gTeensy->menuTimer > MENU_TIMEOUT) {
          Serial.println("TIMEOUT");

          // return to dash state
          gTeensy->currentNode = gTeensy->currentNode->parent;
          gTeensy->displayState = DisplayState::Dash;
        }

        // Up, backward through child highlighted
        if (gBtnPressEvents & BTN_UP) {
          if (gTeensy->currentNode->childIndex > 0) {
            gTeensy->currentNode->childIndex--;
          } else {
            gTeensy->currentNode->childIndex =
                gTeensy->currentNode->children.size() - 1;
          }
          gTeensy->redrawScreen = true;

          gBtnPressEvents &= ~BTN_UP;
        }

        // Right, into child
        if (gBtnPressEvents & BTN_RIGHT) {
          // make sure node has children
          if (gTeensy->currentNode->children[gTeensy->currentNode->childIndex]->
              children.size() > 0) {
            // move to the new node
            gTeensy->currentNode =
                gTeensy->currentNode->children[gTeensy->currentNode->childIndex];
            gTeensy->redrawScreen = true;
          } else {
            // (should show that item has no children)
          }

          gBtnPressEvents &= ~BTN_RIGHT;
        }

        // Down, forward through child highlighted
        if (gBtnPressEvents & BTN_DOWN) {
          if (gTeensy->currentNode->childIndex ==
              gTeensy->currentNode->children.size() - 1) {
            gTeensy->currentNode->childIndex = 0;
          } else {
            gTeensy->currentNode->childIndex++;
          }
          gTeensy->redrawScreen = true;

          gBtnPressEvents &= ~BTN_DOWN;
        }

        // Left, out to parent
        if (gBtnPressEvents & BTN_LEFT) {
          gTeensy->currentNode = gTeensy->currentNode->parent;
          gTeensy->redrawScreen = true;
          if (gTeensy->currentNode->m_nodeType == NodeType::DashHead) {
            gTeensy->displayState = DisplayState::Dash;
          }

          gBtnPressEvents &= ~BTN_LEFT;
        }
        break;
    }

    // Consume all unused events
    gBtnReleaseEvents = BTN_NONE;
    gBtnHeldEvents = BTN_NONE;
  }
}

void _2hzTimer() {
  static uint32_t i, newVal;
  static bool valDecreased, valIncreased;

  // @desc Check if node's observed pins changed in value and must re-render
  for (i = 0; i < gTeensy->currentNode->numPins; i++) {
    newVal = digitalReadFast(gTeensy->currentNode->pins[i]);
    valDecreased =
        ((newVal - gTeensy->currentNode->pinVals[i]) < -(ADC_CHANGE_TOLERANCE));
    valIncreased =
        ((newVal - gTeensy->currentNode->pinVals[i]) > (ADC_CHANGE_TOLERANCE));
    if (valDecreased || valIncreased) {
      // pin/adc val changed by more than ADC_CHANGE_TOLERANCE
      gTeensy->currentNode->pinVals[i] = newVal;
      gTeensy->redrawScreen = true;
    }
  }

  // @desc Inc. menu timeout timer if in the menu state
  if (gTeensy->displayState == DisplayState::Menu) {
    gTeensy->menuTimer += 500; // 1/2 second
  }
}

// @desc Sets button press state & performs debounce
void _50hzTimer() {
  btnDebounce();
}

// @desc Button debounce
void btnDebounce() {
  upButton.update();
  rightButton.update();
  downButton.update();
  leftButton.update();

  gBtnPressEvents |= upButton.pressed();
  gBtnPressEvents |= rightButton.pressed() << 1;
  gBtnPressEvents |= downButton.pressed() << 2;
  gBtnPressEvents |= leftButton.pressed() << 3;

  gBtnReleaseEvents |= upButton.released();
  gBtnReleaseEvents |= rightButton.released() << 1;
  gBtnReleaseEvents |= downButton.released() << 2;
  gBtnReleaseEvents |= leftButton.released() << 3;

  gBtnHeldEvents |= upButton.held();
  gBtnHeldEvents |= rightButton.held() << 1;
  gBtnHeldEvents |= downButton.held() << 2;
  gBtnHeldEvents |= leftButton.held() << 3;
}
