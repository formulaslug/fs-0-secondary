/* @desc Secondary control system for UCSC's FSAE Electric Vehicle
 *       CAN nodeID=4
 */

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

// timer interrupt handlers
void _1sISR();
void _500msISR();
void _20msISR();
void _3msISR();
void timeoutISR();

// declarations of the can message packing functions
CAN_message_t canGetHeartbeat();

void btnDebounce();

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
  constexpr uint32_t k_tftDC0 = 15;
  constexpr uint32_t k_tftCS0 = 10;
  constexpr uint32_t k_tftDC1 = 20;
  constexpr uint32_t k_tftCS1 = 9;
  constexpr uint32_t k_tftMOSI = 11;
  constexpr uint32_t k_tftSCLK = 14;
  constexpr uint32_t k_menuTimeout = 3000000; // in ms

  // Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
  ILI9341_t3 tft[2] = {ILI9341_t3(k_tftCS0, k_tftDC0, 255, k_tftMOSI,
                                  k_tftSCLK),
                       ILI9341_t3(k_tftCS1, k_tftDC1, 255, k_tftMOSI,
                                  k_tftSCLK)};

  /* The SPI bus needs to be initialized before CANopen to avoid a race
   * condition with using the builtin LED pin (the ILI9341_t3 needs to unset
   * that pin as the SPI clock before the CANopen class treats it as an LED).
   */
  constexpr uint32_t k_ID = 0x680;
  constexpr uint32_t k_baudRate = 250000;
  g_canBus = new CANopen(k_ID, k_baudRate);

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

  IntervalTimer _1sInterrupt;
  _1sInterrupt.begin(_1sISR, 1000000);

  IntervalTimer _500msInterrupt;
  _500msInterrupt.begin(_500msISR, 500000);

  IntervalTimer _20msInterrupt;
  _20msInterrupt.begin(_20msISR, 20000);

  IntervalTimer _3msInterrupt;
  _3msInterrupt.begin(_3msISR, 3000);

  /* Used as temporary safe storage for current node pointer, which could
   * otherwise be changed by an ISR
   */
  Node* tempNode;

  Serial.println("[STATUS]: Initialized.");

  while (1) {
    cli();
    // print all transmitted messages
    g_canBus->printTxAll();
    // print all received messages
    g_canBus->printRxAll();
    sei();

    // service main state machine
    switch (g_teensy->displayState) {
      // Display dash only
      case DisplayState::Dash:
        /* Check if should display menu. This btnPress is not counted for menu
         * navigation.
         */
        if (g_btnPressEvents != BTN_NONE) {
          cli();
          // Move to mainMenu node
          g_teensy->currentNode = g_teensy->currentNode->children[0];
          sei();

          // Transition to menu state
          g_teensy->displayState = DisplayState::Menu;
          g_teensy->redrawScreen = true;

          // Consume all button events
          g_btnPressEvents = BTN_NONE;

          // Start timeout
          g_timeoutInterrupt.begin(timeoutISR, k_menuTimeout);

          Serial.println("[EVENT]: Button pressed.");
        }
        break;
      // Display members of menu node tree
      case DisplayState::Menu:
        // Up, backward through child highlighted
        if (g_btnPressEvents & BTN_UP) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, k_menuTimeout);

          cli();
          tempNode = g_teensy->currentNode;
          sei();

          if (tempNode->childIndex > 0) {
            tempNode->childIndex--;
          } else {
            tempNode->childIndex = tempNode->children.size() - 1;
          }

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~BTN_UP;

          Serial.println("[EVENT]: Button <UP> pressed.");
        }

        // Right, into child
        if (g_btnPressEvents & BTN_RIGHT) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, k_menuTimeout);

          // Make sure node has children
          cli();
          tempNode = g_teensy->currentNode;
          sei();

          if (tempNode->children[tempNode->childIndex]->children.size() > 0) {
            // Move to the new node
            cli();
            g_teensy->currentNode = tempNode->children[tempNode->childIndex];
            sei();
            g_teensy->redrawScreen = true;
          } else {
            // (should show that item has no children)
          }

          g_btnPressEvents &= ~BTN_RIGHT;

          Serial.println("[EVENT]: Button <RIGHT> pressed.");
        }

        // Down, forward through child highlighted
        if (g_btnPressEvents & BTN_DOWN) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, k_menuTimeout);

          cli();
          tempNode = g_teensy->currentNode;
          sei();

          if (tempNode->childIndex == tempNode->children.size() - 1) {
            tempNode->childIndex = 0;
          } else {
            tempNode->childIndex++;
          }

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~BTN_DOWN;

          Serial.println("[EVENT]: Button <DOWN> pressed.");
        }

        // Left, out to parent
        if (g_btnPressEvents & BTN_LEFT) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, k_menuTimeout);

          cli();
          g_teensy->currentNode = g_teensy->currentNode->parent;
          tempNode = g_teensy->currentNode;
          sei();

          if (tempNode->m_nodeType == NodeType::DashHead) {
            g_teensy->displayState = DisplayState::Dash;
          }

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~BTN_LEFT;

          Serial.println("[EVENT]: Button <LEFT> pressed.");
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

      Serial.println("[EVENT]: Redrawing screen.");

      g_teensy->redrawScreen = false;
    }
  }
}

/**
 * @desc Performs period tasks every second
 */
void _1sISR() {
  // enqueue heartbeat message to g_canTxQueue
  g_canBus->queueTx(canGetHeartbeat());
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
  g_canBus->processTx();
}

void _3msISR() {
  g_canBus->processRx();
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

/**
 * @desc Writes the node's heartbeat to the CAN bus every 1s
 * @return The packaged message of type CAN_message_t
 */
// TODO: bring this into core controls and have it use a node id that is received by the
//    CANopen constructor
CAN_message_t canGetHeartbeat() {
  static bool didInit = false;
  // heartbeat message formatted with: COB-ID=0x001, len=2
  static CAN_message_t heartbeatMsg = {
    cobid_node4Heartbeat, 0, 2, 0, {0, 0, 0, 0, 0, 0, 0, 0}
  };

  // insert the heartbeat payload on the first call
  if (!didInit) {
    // TODO: add this statically into the initialization of heartbeatMsg
    // populate payload (only once)
    for (uint32_t i = 0; i < 2; ++i) {
      // set in message buff, each byte of the message, from least to most significant
      heartbeatMsg.buf[i] = (payload_heartbeat >> ((1 - i) * 8)) & 0xff;
    }
    didInit = true;
  }
  // return the packed/formatted message
  return heartbeatMsg;
}
