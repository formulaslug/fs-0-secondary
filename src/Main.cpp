// @desc Secondary control system for the UCSC's FSAE Electric Vehicle

#include <cstdint>
#include <atomic>

#include <IntervalTimer.h>

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include "core_controls/ButtonTracker.h"
#include "core_controls/CANopen.h"
#include "libs/font_Arial.h"
#include "libs/ILI9341_t3.h"
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

constexpr uint32_t k_adcChangeTolerance = 3;

// Number of buttons
constexpr uint32_t k_numBtns = 4;

// First pin used by buttons. The rest follow in sequentially increasing order.
constexpr uint32_t k_startBtnPin = 5;

static IntervalTimer g_timeoutInterrupt;

static Teensy* g_teensy;
static CANopen* g_canBus = nullptr;

static std::atomic<uint8_t> g_btnPressEvents{0};
static std::atomic<uint8_t> g_btnReleaseEvents{0};
static std::atomic<uint8_t> g_btnHeldEvents{0};

int main() {
  constexpr uint32_t k_ID = 0x680;
  constexpr uint32_t k_baudRate = 500000;
  g_canBus = new CANopen(k_ID, k_baudRate);

  constexpr uint32_t k_tft0_DC = 15;
  constexpr uint32_t k_tft0_CS = 10;
  constexpr uint32_t k_tft1_DC = 20;
  constexpr uint32_t k_tft1_CS = 9;
  constexpr uint32_t k_menuTimeout = 3000000; // in ms

  // Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
  ILI9341_t3 tft[2] = {ILI9341_t3(k_tft0_CS, k_tft0_DC, 255, 11, 14),
                       ILI9341_t3(k_tft1_CS, k_tft1_DC, 255 ,11, 14)};

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

  g_teensy = new Teensy(head);

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
    switch (g_teensy->displayState) {
      // displaying dash only
      case DisplayState::Dash:
        // check if should display menu, this btnPress is not counted for menu navigation
        if (g_btnPressEvents != BTN_NONE) {
          cli();
          g_teensy->currentNode = g_teensy->currentNode->children[0]; // move to mainMenu node
          sei();

          g_teensy->displayState = DisplayState::Menu; // transition to menu state
          g_teensy->redrawScreen = true;

          // Consume all button events
          g_btnPressEvents = BTN_NONE;

          // Start timeout
          g_timeoutInterrupt.begin(timeoutISR, k_menuTimeout);
        }
        break;
      // displaying members of menu node tree
      case DisplayState::Menu:
        // Up, backward through child highlighted
        if (g_btnPressEvents & BTN_UP) {
          cli();
          if (g_teensy->currentNode->childIndex > 0) {
            g_teensy->currentNode->childIndex--;
          } else {
            g_teensy->currentNode->childIndex =
                g_teensy->currentNode->children.size() - 1;
          }
          sei();

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~BTN_UP;
        }

        // Right, into child
        if (g_btnPressEvents & BTN_RIGHT) {
          // make sure node has children
          cli();
          if (g_teensy->currentNode->children[g_teensy->currentNode->childIndex]->
              children.size() > 0) {
            // move to the new node
            g_teensy->currentNode =
                g_teensy->currentNode->children[g_teensy->currentNode->childIndex];
            g_teensy->redrawScreen = true;
          } else {
            // (should show that item has no children)
          }
          sei();

          g_btnPressEvents &= ~BTN_RIGHT;
        }

        // Down, forward through child highlighted
        if (g_btnPressEvents & BTN_DOWN) {
          cli();
          if (g_teensy->currentNode->childIndex ==
              g_teensy->currentNode->children.size() - 1) {
            g_teensy->currentNode->childIndex = 0;
          } else {
            g_teensy->currentNode->childIndex++;
          }
          sei();

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~BTN_DOWN;
        }

        // Left, out to parent
        if (g_btnPressEvents & BTN_LEFT) {
          cli();
          g_teensy->currentNode = g_teensy->currentNode->parent;
          if (g_teensy->currentNode->m_nodeType == NodeType::DashHead) {
            g_teensy->displayState = DisplayState::Dash;
          }
          sei();

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~BTN_LEFT;
        }
        break;
    }

    // Consume all unused events
    g_btnReleaseEvents = BTN_NONE;
    g_btnHeldEvents = BTN_NONE;

    // Update display
    if (g_teensy->redrawScreen) {
      // Execute draw function for node
      cli();
      g_teensy->currentNode->draw(tft);
      sei();

      g_teensy->redrawScreen = false;
    }
  }
}

void _500msISR() {
  static uint32_t i, newVal;
  static bool valDecreased, valIncreased;
  static Node* tempNode;

  tempNode = g_teensy->currentNode;

  // Check if node's observed pins changed in value and must re-render
  for (i = 0; i < tempNode->numPins; i++) {
    newVal = digitalReadFast(tempNode->pins[i]);
    valDecreased = newVal - tempNode->pinVals[i] < -k_adcChangeTolerance;
    valIncreased = newVal - tempNode->pinVals[i] > k_adcChangeTolerance;
    if (valDecreased || valIncreased) {
      // pin/adc val changed by more than k_adcChangeTolerance
      tempNode->pinVals[i] = newVal;
      g_teensy->redrawScreen = true;
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
  g_timeoutInterrupt.end();

  // Return to dash state
  g_teensy->currentNode = g_teensy->currentNode->parent;

  g_teensy->displayState = DisplayState::Dash;
  g_teensy->redrawScreen = true;
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

  g_btnPressEvents |= upButton.pressed();
  g_btnPressEvents |= rightButton.pressed() << 1;
  g_btnPressEvents |= downButton.pressed() << 2;
  g_btnPressEvents |= leftButton.pressed() << 3;

  g_btnReleaseEvents |= upButton.released();
  g_btnReleaseEvents |= rightButton.released() << 1;
  g_btnReleaseEvents |= downButton.released() << 2;
  g_btnReleaseEvents |= leftButton.released() << 3;

  g_btnHeldEvents |= upButton.held();
  g_btnHeldEvents |= rightButton.held() << 1;
  g_btnHeldEvents |= downButton.held() << 2;
  g_btnHeldEvents |= leftButton.held() << 3;
}

void canTx() {
  static CAN_message_t txMsg;

  txMsg.len = 8;
  txMsg.id = 0x222;
  for (uint32_t i = 0; i < 8; i++) {
    txMsg.buf[i] = '0' + i;
  }

  for (uint32_t i = 0; i < 6; i++) {
    g_canBus->sendMessage(txMsg);
    txMsg.buf[0]++;
  }
}

void canRx() {
  static CAN_message_t rxMsg;

  while (g_canBus->recvMessage(rxMsg)) {
  }
}
