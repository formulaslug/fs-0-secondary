/*
 * @desc Pimary control system for the UCSC's Formula Slug Electric FSAE Vehicle
 * @dict LV = Low Voltage System, HV = High Voltage System, RTD = Ready-To-Drive
 */

// TODO: Need some button debounce

#include <cstdint>
#include <cstdio>
#include <SPI.h>
#include <ILI9341_t3.h> // display main lib

/* ILI9341.h defines a swap macro that conflicts with the C++ standard library,
 * so this undefines it to avoid problems (it should be using the standard
 * library's std::swap() anyway...)
 */
#undef swap

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include <font_Arial.h>

#include "Node.h"
#include "Teensy.h"

constexpr uint32_t TFT_0_DC = 15;
constexpr uint32_t TFT_0_CS = 10;
constexpr uint32_t TFT_1_DC = 20;
constexpr uint32_t TFT_1_CS = 9;
constexpr uint32_t MENU_TIMEOUT = 5000; // in ms
constexpr uint32_t ADC_CHANGE_TOLERANCE = 3;

// Must be multiple of 20ms (1/50s)
constexpr uint32_t SUCCESSFUL_PRESS_TIME = 100;

constexpr uint32_t NUM_BTNS = 4;
constexpr uint32_t START_BTN_PIN = 2;

void _2hzTimer();
void _50hzTimer();
int btnDebounce();
void drawDash(Node* node);
void drawMenuHead(Node* node);
void drawNodeMenu(Node* node);


// Globals
static Teensy* teensy;
// Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
static ILI9341_t3 tft[2] = {ILI9341_t3(TFT_0_CS, TFT_0_DC),
                            ILI9341_t3(TFT_1_CS, TFT_1_DC)};
/* ILI9341_t3 tft = ILI9341_t3(TFT_0_CS, TFT_0_DC); */

IntervalTimer interval50hz;
IntervalTimer interval2hz;

void setup() {
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
  auto head = new Node(); // dash is tree head
  head->m_nodeName = NodeName::DashHead;
  head->draw = drawDash;

  // main menu
  auto menuHead = new Node();
  menuHead->m_nodeName = NodeName::MenuHead;
  menuHead->draw = drawMenuHead;
  menuHead->parent = head;
  head->children.push_back(menuHead);

  // children of menu head
  menuHead->children.push_back(new Node("Sensors"));
  menuHead->children.push_back(new Node("Settings"));
  menuHead->children.push_back(new Node("Other"));

  teensy = new Teensy(head);

  /* NODES: - must have all their attributes defined, but do not need to have
   * children
   */

  // Set the interval timers
  interval50hz.begin(_50hzTimer, 20000);
  /* interval2hz.begin(_2hzTimer, 500000); */
}

// Main control run loop
void loop() {
  switch(teensy->displayState) {
    // displaying dash only
    case DisplayState::Dash:
      // check if should redraw
      if (teensy->contentDidChange) {
        // execute drawDash func. pointed to, to update display
        teensy->currentNode->draw(teensy->currentNode);
        teensy->contentDidChange = false; // clear re-render
      }
      // check if should display menu, this btnPress is not counted for menu navigation
      if (teensy->btnPress) {
      /* if (true) { */
        teensy->currentNode = teensy->currentNode->children[0]; // move to mainMenu node
        teensy->displayState = DisplayState::Menu; // transition to menu state
        teensy->contentDidChange = true; // trigger re-render
        teensy->menuTimer = 0; // clear menu timeout timer
        teensy->btnPress = BTN_NONE; // reset btn press
      }
      break;
    // displaying members of menu node tree
    case DisplayState::Menu:
      if (teensy->contentDidChange) {
        // execute draw func. pointed to in node, to update display
        teensy->currentNode->draw(teensy->currentNode);
        teensy->contentDidChange = false; // clear re-render
      }
      if (teensy->menuTimer > MENU_TIMEOUT) {
        // return to dash state
        teensy->currentNode = teensy->currentNode->parent;
        teensy->displayState = DisplayState::Dash;
      }
      switch (teensy->btnPress) {
        case BTN_0: // up..backward through child highlighted
          if (teensy->currentNode->currentChildIndex > 0) {
            teensy->currentNode->currentChildIndex--;
          } else {
            teensy->currentNode->currentChildIndex = teensy->currentNode->children.size() - 1;
          }
          teensy->contentDidChange = true; // trigger re-render
          teensy->btnPress = BTN_NONE; // reset btn press
          break;
        case BTN_1: // right..into child
          // make sure node has children
          if (teensy->currentNode->children[teensy->currentNode->currentChildIndex]->children.size() > 0) {
            // set parent of this child if not already set
            if (teensy->currentNode->children[teensy->currentNode->currentChildIndex]->parent == nullptr) {
              teensy->currentNode->children[teensy->currentNode->currentChildIndex]->parent = teensy->currentNode;
            }
            // move to the new node
            teensy->currentNode = teensy->currentNode->children[teensy->currentNode->currentChildIndex];
            teensy->contentDidChange = true; // trigger re-render
          } else {
            // (should show that item has no children)
          }
          teensy->btnPress = BTN_NONE; // reset btn press
          break;
        case BTN_2: // down..forward through child highlighted
          teensy->currentNode->currentChildIndex += 1;
          if (teensy->currentNode->currentChildIndex >= teensy->currentNode->children.size()) {
            teensy->currentNode->currentChildIndex = 0;
          }
          teensy->contentDidChange = true; // trigger re-render
          teensy->btnPress = BTN_NONE; // reset btn press
          break;
        case BTN_3: // left..out to parent
          teensy->currentNode = teensy->currentNode->parent;
          teensy->contentDidChange = true;
          if (teensy->currentNode->m_nodeName == NodeName::DashHead) {
            teensy->displayState = DisplayState::Dash;
          }
          teensy->btnPress = BTN_NONE; // reset btn press
          break;
      }
      break;
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
      teensy->contentDidChange = true; // trigger re-render
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



/***********************************************************************************************
* Node drawing functions
***********************************************************************************************/
void drawDash(Node* node) {
  // display 1
  tft[0].fillScreen(ILI9341_BLACK);
  tft[0].setFont(Arial_96);
  tft[0].setCursor(0, 50);
  tft[0].print("XX");
  tft[0].setFont(Arial_28);
  tft[0].setCursor(200, 117);
  tft[0].print("mph");

  // display 2
  tft[1].fillScreen(ILI9341_BLACK);
  tft[1].setFont(Arial_48);
  tft[1].setCursor(10, 10);
  tft[1].print("FULL");
  tft[1].setCursor(10, 80);
  tft[1].print("100");
  tft[1].setCursor(10, 150);
  tft[1].print("SLAMUR");
}

void drawNodeMenu(Node* node) {
  // display 1
  tft[0].fillScreen(ILI9341_BLACK);

  for (uint32_t i = 0; i < node->children.size(); i++) {
    tft[0].setCursor(10, (10 + 52 * i));
    if (i == node->currentChildIndex) {
      // invert display of node
      tft[0].fillRect(0, (0 + 50*i), 350, 50, ILI9341_YELLOW);
      tft[0].setTextColor(ILI9341_BLACK);
      tft[0].print(node->children[i]->name);
      tft[0].setTextColor(ILI9341_YELLOW);
    } else {
      // print regularly
      tft[0].print(node->children[i]->name);
      tft[0].drawFastHLine(0, (50 + 50 * i), 320, ILI9341_YELLOW);
      tft[0].drawFastHLine(0, (51 + 50 * i), 320, ILI9341_YELLOW);
    }
  }

  /* char num = node->currentChildIndex + '0'; */
  /* tft[0].setCursor(100,170); */
  /* tft[0].print({num}); */
  // display 2
  tft[1].fillScreen(ILI9341_BLACK);
  tft[1].setCursor(10,10);
  tft[1].setFont(Arial_20);

  char str[30];
  sprintf(str, "[This is <%s> node data]", node->children[node->currentChildIndex]->name);
  tft[1].print(str);
}

void drawMenuHead(Node* node) {
  drawNodeMenu(node);
}
