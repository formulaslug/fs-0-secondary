// @desc Secondary control system for the UCSC's FSAE Electric Vehicle

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

void _500msISR();
void _20msISR();
void _3msISR();
void timeoutISR();

void btnDebounce();
void canTx();
void canRx();

constexpr uint32_t ADC_CHANGE_TOLERANCE = 3;

// Must be multiple of 20ms (1/50s)
constexpr uint32_t SUCCESSFUL_PRESS_TIME = 100;

// Number of buttons
constexpr uint32_t k_numBtns = 4;

// First pin used by buttons. The rest follow in sequentially increasing order.
constexpr uint32_t k_startBtnPin = 5;

static IntervalTimer gTimeoutInterrupt;

static Teensy* gTeensy;
static CANopen* gCanBus = nullptr;

static std::atomic<uint8_t> gBtnPressEvents{0};
static std::atomic<uint8_t> gBtnReleaseEvents{0};
static std::atomic<uint8_t> gBtnHeldEvents{0};

int main() {
  constexpr uint32_t k_ID = 0x680;
  constexpr uint32_t k_baudRate = 500000;
  gCanBus = new CANopen(k_ID, k_baudRate);

  constexpr uint32_t TFT_0_DC = 15;
  constexpr uint32_t TFT_0_CS = 10;
  constexpr uint32_t TFT_1_DC = 20;
  constexpr uint32_t TFT_1_CS = 9;
  constexpr uint32_t k_menuTimeout = 3000000; // in ms

  // Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
  ILI9341_t3 tft[2] = {ILI9341_t3(TFT_0_CS, TFT_0_DC, 255, 11, 14),
                       ILI9341_t3(TFT_1_CS, TFT_1_DC, 255 ,11, 14)};

  Serial.begin(115200);
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

  IntervalTimer _500msInterrupt;
  _500msInterrupt.begin(_500msISR, 500000);

  IntervalTimer _20msInterrupt;
  _20msInterrupt.begin(_20msISR, 20000);

  IntervalTimer _3msInterrupt;
  _3msInterrupt.begin(_3msISR, 3000);

  while (1) {
    switch (gTeensy->displayState) {
      // displaying dash only
      case DisplayState::Dash:
        // check if should display menu, this btnPress is not counted for menu navigation
        if (gBtnPressEvents != BTN_NONE) {
          cli();
          gTeensy->currentNode = gTeensy->currentNode->children[0]; // move to mainMenu node
          sei();

          gTeensy->displayState = DisplayState::Menu; // transition to menu state
          gTeensy->redrawScreen = true;

          // Consume all button events
          gBtnPressEvents = BTN_NONE;

          // Start timeout
          gTimeoutInterrupt.begin(timeoutISR, k_menuTimeout);
        }
        break;
      // displaying members of menu node tree
      case DisplayState::Menu:
        // Up, backward through child highlighted
        if (gBtnPressEvents & BTN_UP) {
          cli();
          if (gTeensy->currentNode->childIndex > 0) {
            gTeensy->currentNode->childIndex--;
          } else {
            gTeensy->currentNode->childIndex =
                gTeensy->currentNode->children.size() - 1;
          }
          sei();

          gTeensy->redrawScreen = true;

          gBtnPressEvents &= ~BTN_UP;
        }

        // Right, into child
        if (gBtnPressEvents & BTN_RIGHT) {
          // make sure node has children
          cli();
          if (gTeensy->currentNode->children[gTeensy->currentNode->childIndex]->
              children.size() > 0) {
            // move to the new node
            gTeensy->currentNode =
                gTeensy->currentNode->children[gTeensy->currentNode->childIndex];
            gTeensy->redrawScreen = true;
          } else {
            // (should show that item has no children)
          }
          sei();

          gBtnPressEvents &= ~BTN_RIGHT;
        }

        // Down, forward through child highlighted
        if (gBtnPressEvents & BTN_DOWN) {
          cli();
          if (gTeensy->currentNode->childIndex ==
              gTeensy->currentNode->children.size() - 1) {
            gTeensy->currentNode->childIndex = 0;
          } else {
            gTeensy->currentNode->childIndex++;
          }
          sei();

          gTeensy->redrawScreen = true;

          gBtnPressEvents &= ~BTN_DOWN;
        }

        // Left, out to parent
        if (gBtnPressEvents & BTN_LEFT) {
          cli();
          gTeensy->currentNode = gTeensy->currentNode->parent;
          if (gTeensy->currentNode->m_nodeType == NodeType::DashHead) {
            gTeensy->displayState = DisplayState::Dash;
          }
          sei();

          gTeensy->redrawScreen = true;

          gBtnPressEvents &= ~BTN_LEFT;
        }
        break;
    }

    // Consume all unused events
    gBtnReleaseEvents = BTN_NONE;
    gBtnHeldEvents = BTN_NONE;

    // Update display
    if (gTeensy->redrawScreen) {
      // Execute draw function for node
      cli();
      gTeensy->currentNode->draw(tft);
      sei();

      gTeensy->redrawScreen = false;
    }
  }
}

void _500msISR() {
  static uint32_t i, newVal;
  static bool valDecreased, valIncreased;
  static Node* tempNode;

  tempNode = gTeensy->currentNode;

  // Check if node's observed pins changed in value and must re-render
  for (i = 0; i < tempNode->numPins; i++) {
    newVal = digitalReadFast(tempNode->pins[i]);
    valDecreased = newVal - tempNode->pinVals[i] < -ADC_CHANGE_TOLERANCE;
    valIncreased = newVal - tempNode->pinVals[i] > ADC_CHANGE_TOLERANCE;
    if (valDecreased || valIncreased) {
      // pin/adc val changed by more than ADC_CHANGE_TOLERANCE
      tempNode->pinVals[i] = newVal;
      gTeensy->redrawScreen = true;
    }
  }
}

void _20msISR() {
  btnDebounce();
  canTx();
}

void _3msISR() {
  canRx();
}

void timeoutISR() {
  gTimeoutInterrupt.end();

  // Return to dash state
  gTeensy->currentNode = gTeensy->currentNode->parent;

  gTeensy->displayState = DisplayState::Dash;
  gTeensy->redrawScreen = true;
}

void btnDebounce() {
  static ButtonTracker<4> upButton(k_startBtnPin, false);
  static ButtonTracker<4> rightButton(k_startBtnPin + 1, false);
  static ButtonTracker<4> downButton(k_startBtnPin + 2, false);
  static ButtonTracker<4> leftButton(k_startBtnPin + 3, false);

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

void canTx() {
  static CAN_message_t gTxMsg;

  gTxMsg.len = 8;
  gTxMsg.id = 0x222;
  for (uint32_t i = 0; i < 8; i++) {
    gTxMsg.buf[i] = '0' + i;
  }

  for (uint32_t i = 0; i < 6; i++) {
    gCanBus->sendMessage(gTxMsg);
    gTxMsg.buf[0]++;
  }
}

void canRx() {
  static CAN_message_t gRxMsg;

  while (gCanBus->recvMessage(gRxMsg)) {
  }
}
