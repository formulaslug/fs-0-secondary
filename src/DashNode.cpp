// Copyright (c) 2016-2017 Formula Slug. All Rights Reserved.

#include "DashNode.h"

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include <font_Arial.h>

DashNode::DashNode(const char* nameStr) : Node(nameStr) {}

void DashNode::draw(Display* displays) {
  // display 1
  displays[0].fillScreen(ILI9341_BLACK);
  displays[0].setFont(Arial_96);
  displays[0].setCursor(0, 50);
  displays[0].print("XX");
  displays[0].setFont(Arial_28);
  displays[0].setCursor(200, 117);
  displays[0].print("mph");

  // display 2
  displays[1].fillScreen(ILI9341_BLACK);
  displays[1].setFont(Arial_48);
  displays[1].setCursor(10, 10);
  displays[1].print("FULL");
  displays[1].setCursor(10, 80);
  displays[1].print("100");
  displays[1].setCursor(10, 150);
  displays[1].print("SLAMUR");
}
