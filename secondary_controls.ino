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

// Pre-proc. Dirs.
#define NUM_LEDS 1
enum States {
  LV_STARTUP = 0,
  LV_ACTIVE,
  HV_SD,
  HV_STARTUP,
  HV_ACTIVE,
  RTD_SD,
  RTD_STARTUP,
  RTD_ACTIVE,
};

typedef struct Vehicle { // the main attributes of the vehicle
  uint8_t state;
} Vehicle;

// Globals
Vehicle vehicle = {};

// Instantiate display obj; use hardware SPI (#13, #12, #11)
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC);

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
  vehicle.state = LV_STARTUP;
}

char s[20];
int i = 0;
// Main control run loop
void loop() {

  if (vehicle.state == LV_STARTUP) {
    Serial.println("Setup complete");
    vehicle.state = LV_ACTIVE;

    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 4);
    tft.print("Hello, race carssss!");


    /* tft.fillScreen(ILI9341_BLACK); */
    /* tft.setCursor(20, 20); */
    /* tft.print("Hello"); */
  }

  /* // Vehicle's main state machine (FSM) */
  /* switch (vehicle.state) { */
  /*   case LV_STARTUP: */
  /*     // perform LV_STARTUP functions */
  /*       // HERE */
  /*     // transition to LV_ACTIVE */
  /*     vehicle.state = LV_ACTIVE; */
  /*     break; */
  /*   case LV_ACTIVE: */
  /*     // set led feedback */
  /*     vehicle.leds[BLUE] = LED_ON; */
  /*     vehicle.leds[YELLOW] = LED_OFF; */
  /*     vehicle.leds[RED] = LED_OFF; */
  /*     // wait to move to HV_STARTUP */
  /*     if (digitalRead(buttonPins[HV_TOGGLE]) == LOW) { */
  /*       vehicle.state = HV_STARTUP; */
  /*     } */
  /*     break; */
  /*   case HV_SD: */
  /*     // perform HV_SD functions */
  /*       // HERE */
  /*     // transition to LV_ACTIVE */
  /*     vehicle.state = LV_ACTIVE; */
  /*     break; */
  /*   case HV_STARTUP: */
  /*     // perform LV_STARTUP functions */
  /*       // HERE */
  /*     // transition to LV_ACTIVE */
  /*     vehicle.state = HV_ACTIVE; */
  /*     break; */
  /*   case HV_ACTIVE: */
  /*     // set led feedback */
  /*     vehicle.leds[BLUE] = LED_ON; */
  /*     vehicle.leds[YELLOW] = LED_ON; */
  /*     vehicle.leds[RED] = LED_OFF; */
  /*     // wait to move to RTD_STARTUP until user input */
  /*     if (digitalRead(buttonPins[RTD_TOGGLE]) == LOW) { */
  /*       vehicle.state = RTD_STARTUP; */
  /*     } else if (digitalRead(buttonPins[HV_TOGGLE]) == LOW) { */
  /*       // Or move back to LV active */
  /*       vehicle.state = HV_SD; */
  /*     } */
  /*     break; */
  /*   case RTD_SD: */
  /*     // perform HV_SD functions */
  /*       // HERE */
  /*     // transition to LV_ACTIVE */
  /*     vehicle.state = HV_ACTIVE; */
  /*     break; */
  /*   case RTD_STARTUP: */
  /*     // perform LV_STARTUP functions */
  /*       // HERE */
  /*     // transition to LV_ACTIVE */
  /*     vehicle.i = 0; */
  /*     vehicle.state = RTD_ACTIVE; */
  /*     break; */
  /*   case RTD_ACTIVE: */
  /*     // show entire system is hot */
  /*     if (vehicle.i <= 1) { */
  /*       vehicle.leds[BLUE] = LED_ON; */
  /*       vehicle.leds[YELLOW] = LED_ON; */
  /*       vehicle.leds[RED] = LED_ON; */
  /*     } else { */
  /*       // get speed */
  /*       vehicle.dynamics.torque = (int)(analogRead(TORQUE_INPUT) / 2); */
  /*       // show speed */
  /*       vehicle.leds[SPEED] = ~vehicle.leds[SPEED]; */
  /*       // wait to transition back */
  /*       if (digitalRead(buttonPins[RTD_TOGGLE]) == LOW) { */
  /*         // move back to HV_ACTIVE */
  /*         vehicle.leds[SPEED] = LED_OFF; */
  /*         vehicle.dynamics.torque = 50; */
  /*         vehicle.state = RTD_SD; */
  /*       } */
  /*     } */
  /*     break; */
  /* } */
}


