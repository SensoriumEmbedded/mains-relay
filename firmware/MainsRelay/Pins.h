/*
  The relay pins, one per channel, channel 1 first.  Each drives a
  solid-state relay's DC input directly: high powers the machine.  The
  relay's input holds a floating pin low, so the relay is off while the
  firmware isn't running.

  Pin 2 is the pin README.md's wiring names.
*/
#pragma once
#include <stdint.h>

static const uint8_t RelayPins[] = { 2 };
static const uint8_t RelayChannels = sizeof RelayPins / sizeof RelayPins[0];

static const uint8_t StatusLedPin = 13;   // the Teensy's LED: lit while any channel is cut
