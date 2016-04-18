#include "MenuNode.h"

#include <cstdio>

/* Available sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32, 40, 60,
 *                  72, 96
 */
#include <font_Arial.h>

MenuNode::MenuNode(const char* nameStr) : Node(nameStr) {}

void MenuNode::draw(Display* displays) {
  // display 1
  displays[0].fillScreen(ILI9341_BLACK);

  for (uint32_t i = 0; i < children.size(); i++) {
    displays[0].setCursor(10, (10 + 52 * i));
    if (i == childIndex) {
      // invert display of node
      displays[0].fillRect(0, (0 + 50 * i), 350, 50, ILI9341_YELLOW);
      displays[0].setTextColor(ILI9341_BLACK);
      displays[0].print(children[i]->name);
      displays[0].setTextColor(ILI9341_YELLOW);
    } else {
      // print regularly
      displays[0].print(children[i]->name);
      displays[0].drawFastHLine(0, (50 + 50 * i), 320, ILI9341_YELLOW);
      displays[0].drawFastHLine(0, (51 + 50 * i), 320, ILI9341_YELLOW);
    }
  }

  /*
   * char num = node->childIndex + '0';
   * displays[0].setCursor(100,170);
   * displays[0].print({num});
   */
  // display 2
  displays[1].fillScreen(ILI9341_BLACK);
  displays[1].setCursor(10, 10);
  displays[1].setFont(Arial_20);

  char str[30];
  sprintf(str, "[This is <%s> node data]", children[childIndex]->name);
  displays[1].print(str);
}
