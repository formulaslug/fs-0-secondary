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
#include <SPI.h>
#include <ILI9341_t3.h> // display main lib
#include <font_Arial.h>

/***********************************************************************************************
* #defs and Enums
***********************************************************************************************/
#define TFT_DC  9
#define TFT_CS 10
#define MAX_NUM_CHILDREN 10
#define MAX_NUM_PINS 10
#define MENU_TIMEOUT 10000 // in ms...=10s
#define ADC_CHANGE_TOLERANCE 3
#define SUCCESSFUL_PRESS_TIME 200 
#define NUM_LEDS 1
enum States {
  DISPLAYING_DASH = 0,
  DISPLAYING_MENU
};
enum ButtonStates {
  BTN_NONE = 0,
  BTN_0, // =1
  BTN_1, // =2
  BTN_2, // =3
  BTN_3  // =4
};
enum NodeNames {
  DASH_HEAD = 0,
  MENU_HEAD
};


/***********************************************************************************************
* Classes
***********************************************************************************************/
class Node {
  public:
    int name;
    void (*draw)(Node *);
    Node * children[MAX_NUM_CHILDREN];
    int numChildren;
    int currentChildIndex;
    Node * parent;
    int pins[MAX_NUM_PINS];
    int pinVals[MAX_NUM_PINS];
    int numPins;
};

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
void _20hzTimer();
int btnDebounce();
void drawDash(Node *);


/***********************************************************************************************
* Globals
***********************************************************************************************/
Teensy teensy = {};
// Instantiate display obj and properties; use hardware SPI (#13, #12, #11)
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC);


/***********************************************************************************************
* Main lifetime functions
***********************************************************************************************/
void setup() {
  Serial.begin(9600);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  /* tft.setTextSize(2); */
  tft.setFont(Arial_20);
  tft.setCursor(0, 4); // (x,y)

  // init Teensy pins
  // create the node tree
  Node dashHead;
  dashHead.name = DASH_HEAD;
  dashHead.draw = drawDash;
  dashHead.numChildren = 0;
  dashHead.currentChildIndex = 0;
  dashHead.parent = NULL;
  dashHead.numPins = 0;

  // init the vehicle
  teensy.state = DISPLAYING_DASH;
  teensy.currentNode = &dashHead; // set to head of tree
  teensy.contentDidChange = true; // trigger an initial rendering
  teensy.btnPress = BTN_NONE;
  // NODES: - must have all their attributes defined, but do not need to have children
  //    - must init currentChildIndex=0
}

// Main control run loop
void loop() {
  switch(teensy.state) {
    // displaying dash only
    case DISPLAYING_DASH:
      // check if should redraw
      if (teensy.contentDidChange) {
        // execute drawDash func. pointed to, to update display
        tft.print("<drawing dash>");
        teensy.currentNode->draw(teensy.currentNode);
        teensy.contentDidChange = false; // clear re-render
      }
      // check if should display menu, this btnPress is not counted for menu navigation
      if (teensy.btnPress) {
        teensy.currentNode = teensy.currentNode->children[0]; // move to mainMenu node
        teensy.state = DISPLAYING_MENU; // transition to menu state
        teensy.contentDidChange = true; // trigger re-render
        teensy.menuTimer = 0; // clear menu timeout timer
        tft.print("<trans. to menu>");
      }
      break;
    // displaying members of menu node tree
    case DISPLAYING_MENU:
      if (teensy.contentDidChange) {
        tft.print("<displaying menu>");
        // execute draw func. pointed to in node, to update display
        teensy.currentNode->draw(teensy.currentNode);
        teensy.contentDidChange = false; // clear re-render
      }
      if (teensy.menuTimer > MENU_TIMEOUT) {
        // return to dash state
        teensy.state = DISPLAYING_DASH;
      }
      switch (teensy.btnPress) {
        case BTN_0: // up..backward through child highlighted
          teensy.currentNode->currentChildIndex =
            (teensy.currentNode->currentChildIndex - 1) % teensy.currentNode->numChildren;
          teensy.contentDidChange = true; // trigger re-render
          break;
        case BTN_1: // right..into child
          // make sure node has children
          if (teensy.currentNode->numChildren > 0) {
            // set parent of this child if not already set
            if (teensy.currentNode->children[teensy.currentNode->currentChildIndex]->parent == NULL) {
              teensy.currentNode->children[teensy.currentNode->currentChildIndex]->parent = teensy.currentNode;
            }
            // move to the new node
            teensy.currentNode = teensy.currentNode->children[teensy.currentNode->currentChildIndex];
            teensy.contentDidChange = true; // trigger re-render
          }
          break;
        case BTN_2: // down..forward through child highlighted
          teensy.currentNode->currentChildIndex =
            (teensy.currentNode->currentChildIndex + 1) % teensy.currentNode->numChildren;
          teensy.contentDidChange = true; // trigger re-render
          break;
        case BTN_3: // left..out to parent
          teensy.currentNode = teensy.currentNode->parent;
          teensy.contentDidChange = true;
          if (teensy.currentNode->name == DASH_HEAD) {
            teensy.state = DISPLAYING_DASH;
          }
          break;
      }
      break;
  }
  /* if (teensy.state == DISPLAYING_DASH) { */
  /*   Serial.println("Setup complete"); */
  /*   teensy.state = DISPLAYING_MENU; */
  /*  */
  /*   tft.fillScreen(ILI9341_BLACK); */
  /*   tft.setCursor(0, 4); */
  /*   tft.print("C++ iss go."); */
  /*  */
  /*   tft.fillScreen(ILI9341_BLACK); */
  /*   tft.setCursor(20, 20); */
  /*   tft.print("Hello"); */
  /* } */
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
    teensy.menuTimer += 200; // 1/5 second
  }
}

// @desc Sets button press state & performs debounce
void _20hzTimer()
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
  static int prevState = BTN_NONE, state, timer;
  // !!!!!!!!!!
  // READ STATE AND SET TO `state`
  // !!!!!!!!!!
  if (timer >= SUCCESSFUL_PRESS_TIME) {
    // btn successfully pressed
    prevState = BTN_NONE;
    timer = 0;
    return state; 
  } else if (prevState != state) {
    timer = 0; // btn state changed, reset
  } else {
    timer += 50; // btn state persisted..+50ms (1/20s)
  }
  prevState = state;
  return BTN_NONE; // return none until a press is evaled
}



/***********************************************************************************************
* Node drawing functions
***********************************************************************************************/
void drawDash(Node *)
{
  tft.print("[Displaying Dash]");
}
