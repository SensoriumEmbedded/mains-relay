/*
  Mains Relay: a USB-controlled mains switch for a bench power supply.
  Copyright (c) 2026 Travis Smith.  MIT license: see LICENSE.
  Host protocol on the USB serial port: docs/Host_Protocol.md.

  Written for a Teensy 3.1/3.2 (MK20DX256): BootHook.cpp, ChipSerial.h,
  HostConnected() and the watchdog below are specific to that chip.
*/
#include <Arduino.h>
#include "BuildStamp.h"
#include "Engine.h"
#include "HostPort.h"
#include "ChipSerial.h"
#include "Pins.h"

#if !defined(__MK20DX256__)
#error "Mains Relay is written for a Teensy 3.1/3.2 (MK20DX256)"
#endif

#define MR_FW_VERSION "1.0.0"

// A hang resets the chip after this long, and the boot hook turns every relay back on
static const uint16_t WatchdogMs = 1000;

static uint64_t Now64()
{
   static uint32_t Last = 0;
   static uint64_t High = 0;
   uint32_t now = micros();
   if (now < Last) High += (uint64_t)1 << 32;
   Last = now;
   return High + now;
}

// The host has the port open: DTR, and the bus not suspended.  The core never
// clears DTR when the cable is pulled or the PC sleeps; the bus suspends then,
// so that counts as the host closing the port.  There's no suspend flag to
// poll (the core clears USB0_ISTAT's SLEEP bit in its interrupt), but a live
// bus has a start-of-frame every 1 ms, and the frame number stops when the
// bus suspends (after 3 ms idle) or the cable is pulled.
static bool HostConnected()
{
   static uint16_t LastFrame = 0;
   static uint32_t LastChange = 0;
   uint16_t frame = (uint16_t)(USB0_FRMNUML | ((USB0_FRMNUMH & 7) << 8));
   uint32_t now = millis();
   if (frame != LastFrame)
   {
      LastFrame = frame;
      LastChange = now;
   }
   else if (now - LastChange > 5) return false;
   return Serial.dtr();
}

class TeensyHal : public Hal
{
public:
   uint64_t NowUs() override { return Now64(); }
   void SetPower(uint8_t Ch, bool On) override { digitalWrite(RelayPins[Ch], On ? HIGH : LOW); }
   void WriteLine(const char *Line) override
   {
      if (!HostConnected()) return;   // nobody listening
      Serial.print(Line);
      Serial.print('\n');
   }
};

static TeensyHal TheHal;
static Identity  Id;
static Engine    Relay(TheHal, Id, RelayChannels);
static HostPort  Port(Relay);

static void WatchdogBegin()
{
   noInterrupts();
   WDOG_UNLOCK = WDOG_UNLOCK_SEQ1;
   WDOG_UNLOCK = WDOG_UNLOCK_SEQ2;
   __asm__ volatile("nop");
   __asm__ volatile("nop");
   WDOG_PRESC = 0;
   WDOG_TOVALH = 0;
   WDOG_TOVALL = WatchdogMs;   // counts the 1 kHz LPO clock (STCTRLH CLKSRC = 0)
   WDOG_STCTRLH = WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN;
   interrupts();
}

// Refreshed every 100 ms rather than every loop, so a refresh never lands
// right after the last one or right after the enable.
static void WatchdogKick()
{
   static uint32_t Last = 0;
   uint32_t now = millis();
   if (now - Last < 100) return;
   Last = now;
   noInterrupts();
   WDOG_REFRESH = 0xA602;
   WDOG_REFRESH = 0xB480;
   interrupts();
}

void setup()
{
   // The boot hook (BootHook.cpp) already drove every relay on
   pinMode(StatusLedPin, OUTPUT);
   digitalWrite(StatusLedPin, LOW);

   Id.Fw = MR_FW_VERSION;
   Id.Build = BuildStamp_Get();   // the newest compile time of the sketch's files (BuildStamp.h)
   Id.Chip = MR_ChipSerialNum();
   Id.Mcu = MR_MCU;
   Relay.Begin();

   Serial.begin(9600);   // USB CDC: the rate is ignored.  Never 134, which reboots into the bootloader.
   WatchdogBegin();
}

void loop()
{
   WatchdogKick();
   Port.SetConnected(HostConnected());   // a drop restores every channel; bytes while closed are discarded
   uint64_t now = Now64();
   for (int n = 0; n < 64 && Serial.available() > 0; n++)
   {
      int c = Serial.read();
      if (c < 0) break;
      Port.Byte((uint8_t)c, now);
   }
   Port.Tick(now);
   Relay.Poll();
   digitalWrite(StatusLedPin, Relay.AnyOff() ? HIGH : LOW);
}
