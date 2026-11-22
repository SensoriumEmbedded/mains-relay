/*
  Boot hooks.

  startup_early_hook() runs first thing after reset (ResetHandler in
  cores/teensy3/mk20dx128.c), before the clocks, the RAM initialization and
  the core's ~300 ms USB start-up wait, so every relay is back on within
  microseconds of a reset.  It replaces the core's default hook, so it does
  that hook's job too: the watchdog write that must land in the window
  right after the unlock.  The GPIO port clocks aren't on yet, so it turns
  them on.  It may touch nothing in RAM: .data and .bss aren't set up yet.

  startup_middle_hook() runs before usb_init(), so the USB strings are
  final before the host reads them.
*/
#include <Arduino.h>
#include "BuildStamp.h"
#include "Pins.h"

extern "C" void UsbNames_AppendChipId(void);

extern "C" void startup_early_hook(void)
{
   WDOG_STCTRLH = WDOG_STCTRLH_ALLOWUPDATE;   // the core's default: off until setup() starts it
   SIM_SCGC5 |= SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE;
   for (uint8_t i = 0; i < RelayChannels; i++)
   {
      pinMode(RelayPins[i], OUTPUT);
      digitalWrite(RelayPins[i], HIGH);
   }
}

extern "C" void startup_middle_hook(void)
{
   UsbNames_AppendChipId();
}
