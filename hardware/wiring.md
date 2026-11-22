# Wiring

> **PLACEHOLDER: to be supplied.**

The fixed points, from [README.md](README.md):

- inlet → supply switch ‖ relay → fuse → supply, on the hot (black) wire
- Teensy pin → relay input +, Teensy GND → relay input −
- Teensy powered from USB only. No connection to the supply's DC outputs

Pin number, wire gauge, mounting and separation details: to be supplied. Until then, the firmware drives pin 2 (`RelayPins` in `firmware/MainsRelay/Pins.h`); change it there to match the wiring.
