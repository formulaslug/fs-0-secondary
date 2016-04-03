/*
 * @desc Pimary control system for the UCSC's Formula Slug Electric FSAE Vehicle
 * @dict LV = Low Voltage System, HV = High Voltage System, RTD = Ready-To-Drive
 */
// Rev notes: Need some button debounce

/***********************************************************************************************
* External..
***********************************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <SPI.h>
#include <ILI9341_t3.h> // display main lib
#include <font_Arial.h> // Available sizes: 8,9,10,11,12,13,14,16,18,20,24,28,32,40,60,72,96

/***********************************************************************************************
* #defs and Enums
***********************************************************************************************/
#define TFT_0_DC 15
#define TFT_0_CS 10
#define TFT_1_DC 20
#define TFT_1_CS 9
#define MAX_NUM_CHILDREN 10
#define MAX_NUM_PINS 10
#define MENU_TIMEOUT 5000 // in ms
#define ADC_CHANGE_TOLERANCE 3
#define SUCCESSFUL_PRESS_TIME 100 // must be multiple of 20ms..(1/50)s
#define NUM_BTNS 4
#define START_BTN_PIN 2
#define MAX_NODE_NAME_CHARS 20
enum States {
  DISPLAYING_DASH = 0,
  DISPLAYING_MENU
};
enum ButtonStates {
  BTN_NONE = 0,
  BTN_0, // =1, up
  BTN_1, // =2, right
  BTN_2, // =3, down
  BTN_3  // =4, left
};
enum NodeNames {
  DASH_HEAD = 0,
  MENU_HEAD,
  NAME_NONE,
};


/***********************************************************************************************
* Classes
***********************************************************************************************/
class Node {
  public:
    int _name = NAME_NONE;
    char name[MAX_NODE_NAME_CHARS+1] = {};
    void (*draw) (Node * node) = NULL; // draw function pointer
    Node * children[MAX_NUM_CHILDREN] = {};
    int numChildren = 0;
    int currentChildIndex = 0;
    Node * parent = NULL;
    int pins[MAX_NUM_PINS] = {};
    int pinVals[MAX_NUM_PINS] = {};
    int numPins = 0;
    Node(char * nameStr);
};

Node::Node (char * nameStr)
{
  if (nameStr != NULL) {
    strcpy(name, nameStr);
  } else {
    strcpy(name, "- - no name - -");
  }
}

class Teensy {
  public:
    int state;
    Node * currentNode;
    int menuTimer;
    bool contentDidChange;
    int btnPress; // carries the current unservices state of the buttons
};

/***********************************************************************************************
* Function prototype declarations
***********************************************************************************************/
void _2hzTimer();
void _50hzTimer();
int btnDebounce();
void drawDash(Node * node);
void drawMenuHead(Node * node);
void drawNodeMenu(Node * node);


/***********************************************************************************************
* Globals
***********************************************************************************************/
Teensy teensy = {};
// Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
ILI9341_t3 tft[2] = {ILI9341_t3(TFT_0_CS, TFT_0_DC), ILI9341_t3(TFT_1_CS, TFT_1_DC)};
/* ILI9341_t3 tft = ILI9341_t3(TFT_0_CS, TFT_0_DC); */

IntervalTimer interval50hz;
IntervalTimer interval2hz;


/***********************************************************************************************
* Main lifetime functions
***********************************************************************************************/
void setup() {
  Serial.begin(9600);
  int i;
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
  Node * head = new Node(NULL); // dash is tree head
  head->_name = DASH_HEAD;
  head->draw = drawDash;
  // main menu
  Node * menuHead = new Node(NULL);
  menuHead->_name = MENU_HEAD;
  menuHead->draw = drawMenuHead;
  menuHead->parent = head;
  head->children[0] = menuHead;
  // children of menu head
  char name1[MAX_NODE_NAME_CHARS+1] = "Sensors";
  char name2[MAX_NODE_NAME_CHARS+1] = "Settings";
  char name3[MAX_NODE_NAME_CHARS+1] = "Other";
  Node * child1 = new Node(name1);
  Node * child2 = new Node(name2);
  Node * child3 = new Node(name3);
  menuHead->children[0] = child1;
  menuHead->children[1] = child2;
  menuHead->children[2] = child3;
  menuHead->numChildren = 3;


  // init the vehicle
  teensy.state = DISPLAYING_DASH;
  teensy.currentNode = head; // set to head of tree
  teensy.contentDidChange = true; // trigger an initial rendering
  teensy.btnPress = BTN_NONE;
  // NODES: - must have all their attributes defined, but do not need to have children
  //    - must init currentChildIndex=0

  // Set the interval timers
  interval50hz.begin(_50hzTimer, 20000);
  /* interval2hz.begin(_2hzTimer, 500000); */
}

// Main control run loop
void loop() {
  switch(teensy.state) {
    // displaying dash only
    case DISPLAYING_DASH:
      // check if should redraw
      if (teensy.contentDidChange) {
        // execute drawDash func. pointed to, to update display
        teensy.currentNode->draw(teensy.currentNode);
        teensy.contentDidChange = false; // clear re-render
      }
      // check if should display menu, this btnPress is not counted for menu navigation
      if (teensy.btnPress) {
      /* if (true) { */
        teensy.currentNode = teensy.currentNode->children[0]; // move to mainMenu node
        teensy.state = DISPLAYING_MENU; // transition to menu state
        teensy.contentDidChange = true; // trigger re-render
        teensy.menuTimer = 0; // clear menu timeout timer
        teensy.btnPress = BTN_NONE; // reset btn press
      }
      break;
    // displaying members of menu node tree
    case DISPLAYING_MENU:
      if (teensy.contentDidChange) {
        // execute draw func. pointed to in node, to update display
        teensy.currentNode->draw(teensy.currentNode);
        teensy.contentDidChange = false; // clear re-render
      }
      if (teensy.menuTimer > MENU_TIMEOUT) {
        // return to dash state
        teensy.currentNode = teensy.currentNode->parent;
        teensy.state = DISPLAYING_DASH;
      }
      switch (teensy.btnPress) {
        case BTN_0: // up..backward through child highlighted
          teensy.currentNode->currentChildIndex -= 1;
          if (teensy.currentNode->currentChildIndex < 0) {
            teensy.currentNode->currentChildIndex = teensy.currentNode->numChildren - 1;
          }
          teensy.contentDidChange = true; // trigger re-render
          teensy.btnPress = BTN_NONE; // reset btn press
          break;
        case BTN_1: // right..into child
          // make sure node has children
          if (teensy.currentNode->children[teensy.currentNode->currentChildIndex]->numChildren > 0) {
            // set parent of this child if not already set
            if (teensy.currentNode->children[teensy.currentNode->currentChildIndex]->parent == NULL) {
              teensy.currentNode->children[teensy.currentNode->currentChildIndex]->parent = teensy.currentNode;
            }
            // move to the new node
            teensy.currentNode = teensy.currentNode->children[teensy.currentNode->currentChildIndex];
            teensy.contentDidChange = true; // trigger re-render
          } else {
            // (should show that item has no children)
          }
          teensy.btnPress = BTN_NONE; // reset btn press
          break;
        case BTN_2: // down..forward through child highlighted
          teensy.currentNode->currentChildIndex += 1;
          if (teensy.currentNode->currentChildIndex >= teensy.currentNode->numChildren) {
            teensy.currentNode->currentChildIndex = 0;
          }
          teensy.contentDidChange = true; // trigger re-render
          teensy.btnPress = BTN_NONE; // reset btn press
          break;
        case BTN_3: // left..out to parent
          teensy.currentNode = teensy.currentNode->parent;
          teensy.contentDidChange = true;
          if (teensy.currentNode->_name == DASH_HEAD) {
            teensy.state = DISPLAYING_DASH;
          }
          teensy.btnPress = BTN_NONE; // reset btn press
          break;
      }
      break;
  }
}

/***********************************************************************************************
* Timing-based functions
***********************************************************************************************/
void _2hzTimer()
{
  static int i, newVal;
  static bool valDecreased, valIncreased;

  // @desc Check if node's observed pins changed in value and must re-render
  for (i = 0; i < teensy.currentNode->numPins; i++) {
    newVal = digitalRead(teensy.currentNode->pins[i]);
    valDecreased = ((newVal - teensy.currentNode->pinVals[i]) < -(ADC_CHANGE_TOLERANCE));
    valIncreased = ((newVal - teensy.currentNode->pinVals[i]) > (ADC_CHANGE_TOLERANCE));
    if (valDecreased || valIncreased) {
      // pin/adc val changed by more than ADC_CHANGE_TOLERANCE
      teensy.currentNode->pinVals[i] = newVal;
      teensy.contentDidChange = true; // trigger re-render
    }
  }

  // @desc Inc. menu timeout timer if in the menu state
  if (teensy.state == DISPLAYING_MENU) {
    teensy.menuTimer += 500; // 1/2 second
  }
}

// @desc Sets button press state & performs debounce
void _50hzTimer()
{
  // if there isn't a current btn press waiting to be serviced
  if (!teensy.btnPress) {
    teensy.btnPress = btnDebounce();
  }
}



/***********************************************************************************************
* Timing-based functions
***********************************************************************************************/
// @desc Button debounce, assumed to be ran in a 20hz timer
int btnDebounce()
{
  static int prevState = BTN_NONE, state, timer, i;

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
void drawDash(Node * node)
{
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
void drawNodeMenu(Node * node)
{
  // display 1
  tft[0].fillScreen(ILI9341_BLACK);
  int i;
  for (i = 0; i < node->numChildren; i++) {
    tft[0].setCursor(10, (10 + 52*i));
    if (i == node->currentChildIndex) {
      // invert display of node
      tft[0].fillRect(0,(0 + 50*i),350,50,ILI9341_YELLOW);
      tft[0].setTextColor(ILI9341_BLACK);
      tft[0].print(node->children[i]->name);
      tft[0].setTextColor(ILI9341_YELLOW);
    } else {
      // print regularly
      tft[0].print(node->children[i]->name);
      tft[0].drawFastHLine(0, (50 + 50*i), 320, ILI9341_YELLOW);
      tft[0].drawFastHLine(0, (51 + 50*i), 320, ILI9341_YELLOW);
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
void drawMenuHead(Node * node)
{
  drawNodeMenu(node);
}
