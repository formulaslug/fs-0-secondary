# Secondary Controls

This repository contains the software for our seconary controller of the Formula Slug 2016 racecar, FS-0.

The style guide repository at https://github.com/formulaslug/styleguide contains our style guide for C and C++ code and formatting scripts.

## TODO
- Add caret to node menu showing whether or not it has children
- increase debounce frequency
- make menu drawing more efficient (fewer complete refreshes)
- fix timeout so that it remembers state and returns to the dash (not just one level back up)
- display primary teensy's current state (in FSM) by reading state changes off the CAN bus. Add this to dash state, tft[1] (the 2nd one)
- flash "Ful Slamur" when throttle picked up off the CANopen bus is at 90% of max
