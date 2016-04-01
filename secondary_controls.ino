/*
 * @desc Pimary control system for the UCSC's Formula Slug Electric FSAE Vehicle
 * @dict LV = Low Voltage System, HV = High Voltage System, RTD = Ready-To-Drive
 */
// Rev notes: Need some button debounce

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SPI.h>
#include <ILI9341_t3.h> // display main lib
#include <font_Arial.h>

#define TFT_DC  9
#define TFT_CS 10
#define MAX_NUM_CHILDREN 10
#define MAX_NUM_PINS 10

// Pre-proc. Dirs.
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
};

// Instantiate display obj; use hardware SPI (#13, #12, #11)
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC);
// Teensy obj to contain all necessary globals
Teensy teensy = {};

// Main setup function
void setup() {
  Serial.begin(9600);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  /* tft.setTextSize(2); */
  int xx = 0;
  tft.setFont(Arial_20);
  tft.setCursor(xx, 4);

  // init Teensy pins

  // init the vehicle
  teensy.state = DISPLAYING_DASH;
}

// Main control run loop
void loop() {

  if (teensy.state == DISPLAYING_DASH) {
    Serial.println("Setup complete");
    teensy.state = DISPLAYING_MENU;

    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 4);
    tft.print("C++ iss go.");


    /* tft.fillScreen(ILI9341_BLACK); */
    /* tft.setCursor(20, 20); */
    /* tft.print("Hello"); */
  }
}
