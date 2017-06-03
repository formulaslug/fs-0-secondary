// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

/* @desc Secondary control system for UCSC's FSAE Electric Vehicle
 *       CAN nodeID=4
 */

#include <stdint.h>

#include <atomic>
#include <memory>

#include <IntervalTimer.h>

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include "DashNode.h"
#include "MenuNode.h"
#include "Teensy.h"
#include "fs-0-core/ButtonTracker.h"
#include "fs-0-core/CANopen.h"
#include "fs-0-core/CANopenPDO.h"
#include "fs-0-core/InterruptMutex.h"
#include "fs-0-core/make_unique.h"
#include "libs/ILI9341_t3.h"
#include "libs/font_Arial.h"

enum ButtonStates {
  kBtnNone = 0x0,
  kBtnUp = 0x1,
  kBtnRight = 0x2,
  kBtnDown = 0x4,
  kBtnLeft = 0x8
};

// timer interrupt handlers
void _1sISR();
void _20msISR();
void _3msISR();
void timeoutISR();

void btnDebounce();

constexpr uint32_t kAdcChangeTolerance = 3;

// Number of buttons
constexpr uint32_t kNumBtns = 4;

// First pin used by buttons. The rest follow in sequentially increasing order.
constexpr uint32_t kStartBtnPin = 5;

static IntervalTimer g_timeoutInterrupt;

static std::unique_ptr<Teensy> g_teensy;

static std::unique_ptr<CANopen> g_canBus;

static std::atomic<uint8_t> g_btnPressEvents{0};
static std::atomic<uint8_t> g_btnReleaseEvents{0};
static std::atomic<uint8_t> g_btnHeldEvents{0};

int main() {
  constexpr uint32_t kTftDC0 = 15;
  constexpr uint32_t kTftCS0 = 10;
  constexpr uint32_t kTftDC1 = 20;
  constexpr uint32_t kTftCS1 = 9;
  constexpr uint32_t kTftMOSI = 11;
  constexpr uint32_t kTftSCLK = 14;
  constexpr uint32_t kMenuTimeout = 3000000;  // in ms

  // Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
  ILI9341_t3 tft[2] = {ILI9341_t3(kTftCS0, kTftDC0, 255, kTftMOSI, kTftSCLK),
                       ILI9341_t3(kTftCS1, kTftDC1, 255, kTftMOSI, kTftSCLK)};

  /* The SPI bus needs to be initialized before CANopen to avoid a race
   * condition with using the builtin LED pin (the ILI9341_t3 needs to unset
   * that pin as the SPI clock before the CANopen class treats it as an LED).
   */
  constexpr uint32_t kID = 0x680;
  constexpr uint32_t kBaudRate = 250000;
  g_canBus = std::make_unique<CANopen>(kID, kBaudRate);

  Serial.begin(115200);

  uint32_t i;
  for (i = 0; i < 2; i++) {
    tft[i].begin();
    tft[i].setRotation(1);
    tft[i].fillScreen(ILI9341_BLACK);
    tft[i].setTextColor(ILI9341_YELLOW);
    /* tft[0].setTextSize(2); */
    tft[i].setFont(Arial_20);
    tft[i].setCursor(0, 4);  // (x,y)
  }

  // init Teensy pins
  // -> init btn pins
  for (i = kStartBtnPin; i < (kStartBtnPin + kNumBtns); i++) {
    pinMode(i, INPUT_PULLUP);
  }

  // create the node tree
  auto head = std::make_unique<DashNode>();  // dash is tree head
  head->m_nodeType = NodeType::DashHead;

  // main menu
  auto menuHead = std::make_unique<MenuNode>();
  menuHead->m_nodeType = NodeType::MenuHead;
  head->addChild(std::move(menuHead));

  auto sensors = std::make_unique<MenuNode>("Sensors");
  sensors->addChild(std::make_unique<Node>("Sensor 1"));
  sensors->addChild(std::make_unique<Node>("Sensor 2"));
  sensors->addChild(std::make_unique<Node>("Sensor 3"));
  menuHead->addChild(std::move(sensors));

  menuHead->addChild(std::make_unique<MenuNode>("Settings"));
  menuHead->addChild(std::make_unique<MenuNode>("Other"));

  g_teensy = std::make_unique<Teensy>(std::move(head));

  /* NODES: - must have all their attributes defined, but do not need to have
   * children
   */

  IntervalTimer _1sInterrupt;
  _1sInterrupt.begin(_1sISR, 1000000);

  IntervalTimer _20msInterrupt;
  _20msInterrupt.begin(_20msISR, 20000);

  IntervalTimer _3msInterrupt;
  _3msInterrupt.begin(_3msISR, 3000);

  /* Used as temporary safe storage for current node pointer, which could
   * otherwise be changed by an ISR
   */
  Node* tempNode;

  InterruptMutex interruptMut;

  Serial.println("[STATUS]: Initialized.");

  while (1) {
    {
      std::lock_guard<InterruptMutex> lock(interruptMut);

      // print all transmitted messages
      g_canBus->printTxAll();
      // print all received messages
      g_canBus->printRxAll();
    }

    // service main state machine
    switch (g_teensy->displayState) {
      // Display dash only
      case DisplayState::Dash:
        /* Check if should display menu. This btnPress is not counted for menu
         * navigation.
         */
        if (g_btnPressEvents != kBtnNone) {
          {
            std::lock_guard<InterruptMutex> lock(interruptMut);

            // Move to mainMenu node
            g_teensy->currentNode = g_teensy->currentNode->children[0].get();
          }

          // Transition to menu state
          g_teensy->displayState = DisplayState::Menu;
          g_teensy->redrawScreen = true;

          // Consume all button events
          g_btnPressEvents = kBtnNone;

          // Start timeout
          g_timeoutInterrupt.begin(timeoutISR, kMenuTimeout);

          Serial.println("[EVENT]: Button pressed.");
        }
        break;
      // Display members of menu node tree
      case DisplayState::Menu:
        // Up, backward through child highlighted
        if (g_btnPressEvents & kBtnUp) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, kMenuTimeout);

          {
            std::lock_guard<InterruptMutex> lock(interruptMut);

            tempNode = g_teensy->currentNode;
          }

          if (tempNode->childIndex > 0) {
            tempNode->childIndex--;
          } else {
            tempNode->childIndex = tempNode->children.size() - 1;
          }

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~kBtnUp;

          Serial.println("[EVENT]: Button <UP> pressed.");
        }

        // Right, into child
        if (g_btnPressEvents & kBtnRight) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, kMenuTimeout);

          // Make sure node has children
          {
            std::lock_guard<InterruptMutex> lock(interruptMut);
            tempNode = g_teensy->currentNode;
          }

          if (tempNode->children[tempNode->childIndex]->children.size() > 0) {
            // Move to the new node
            {
              std::lock_guard<InterruptMutex> lock(interruptMut);
              g_teensy->currentNode =
                  tempNode->children[tempNode->childIndex].get();
            }
            g_teensy->redrawScreen = true;
          } else {
            // (should show that item has no children)
          }

          g_btnPressEvents &= ~kBtnRight;

          Serial.println("[EVENT]: Button <RIGHT> pressed.");
        }

        // Down, forward through child highlighted
        if (g_btnPressEvents & kBtnDown) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, kMenuTimeout);

          {
            std::lock_guard<InterruptMutex> lock(interruptMut);
            tempNode = g_teensy->currentNode;
          }

          if (tempNode->childIndex == tempNode->children.size() - 1) {
            tempNode->childIndex = 0;
          } else {
            tempNode->childIndex++;
          }

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~kBtnDown;

          Serial.println("[EVENT]: Button <DOWN> pressed.");
        }

        // Left, out to parent
        if (g_btnPressEvents & kBtnLeft) {
          // Reset timeout interrupt
          g_timeoutInterrupt.end();
          g_timeoutInterrupt.begin(timeoutISR, kMenuTimeout);

          {
            std::lock_guard<InterruptMutex> lock(interruptMut);

            g_teensy->currentNode = g_teensy->currentNode->parent;
            tempNode = g_teensy->currentNode;
          }

          if (tempNode->m_nodeType == NodeType::DashHead) {
            g_teensy->displayState = DisplayState::Dash;
          }

          g_teensy->redrawScreen = true;

          g_btnPressEvents &= ~kBtnLeft;

          Serial.println("[EVENT]: Button <LEFT> pressed.");
        }
        break;
    }

    // Consume all unused events
    g_btnReleaseEvents = kBtnNone;
    g_btnHeldEvents = kBtnNone;

    // Update display
    if (g_teensy->redrawScreen) {
      // Execute draw function for node
      {
        std::lock_guard<InterruptMutex> lock(interruptMut);
        g_teensy->currentNode->draw(tft);
      }

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
  const HeartbeatMessage heartbeatMessage(kCobid_node4Heartbeat);
  g_canBus->queueTxMessage(heartbeatMessage);
}

void _20msISR() {
  static uint32_t i, newVal;
  static bool valDecreased, valIncreased;
  static Node* tempNode;

  tempNode = g_teensy->currentNode;

  // Check if node's observed pins changed in value and must re-render
  for (i = 0; i < tempNode->numPins; i++) {
    newVal = digitalReadFast(tempNode->pins[i]);
    valDecreased = newVal - tempNode->pinVals[i] < -kAdcChangeTolerance;
    valIncreased = newVal - tempNode->pinVals[i] > kAdcChangeTolerance;
    if (valDecreased || valIncreased) {
      // pin/adc val changed by more than kAdcChangeTolerance
      tempNode->pinVals[i] = newVal;
      g_teensy->redrawScreen = true;
    }
  }

  btnDebounce();
  g_canBus->processTxMessages();
}

void _3msISR() { g_canBus->processRxMessages(); }

void timeoutISR() {
  g_timeoutInterrupt.end();

  // Return to dash state
  g_teensy->currentNode = g_teensy->currentNode->parent;

  g_teensy->displayState = DisplayState::Dash;
  g_teensy->redrawScreen = true;
}

void btnDebounce() {
  static ButtonTracker<4> upButton(kStartBtnPin, false);
  static ButtonTracker<4> rightButton(kStartBtnPin + 1, false);
  static ButtonTracker<4> downButton(kStartBtnPin + 2, false);
  static ButtonTracker<4> leftButton(kStartBtnPin + 3, false);

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
