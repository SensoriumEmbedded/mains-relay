/*
  The host protocol (docs/Host_Protocol.md): command lines in, replies and
  relay changes out, on a clock the caller supplies.  No Arduino
  dependencies, so tests/harness.cpp runs the same code on the PC.
*/
#pragma once
#include <stdint.h>

class Hal
{
public:
   virtual uint64_t NowUs() = 0;
   virtual void SetPower(uint8_t Ch, bool On) = 0;   // Ch counts from 0; On powers the machine
   virtual void WriteLine(const char *Line) = 0;     // a reply or EV line, without its LF
};

struct Identity
{
   const char *Fw = "0.0.0";
   const char *Build = "unknown";
   const char *Mcu = "teensy";
   uint32_t    Chip = 0;
};

class Engine
{
public:
   static const uint8_t  MaxChannels = 4;
   static const uint32_t MinOffMs = 2000;
   static const uint32_t OffDefaultMs = 600000, OffMaxMs = 3600000;
   static const uint32_t CycleDefaultMs = 5000, CycleMaxMs = 60000;

   Engine(Hal &H, const Identity &Id, uint8_t Channels);
   void Begin();                    // every channel on
   void OnLine(const char *Text);   // one complete command line, without its LF
   void OnJunkLine();               // a line that was too long or held a bad byte
   void OnDtrDrop();                // the host closed the port, or the bus suspended
   void Poll();                     // restores power whose time has come; call every loop
   bool AnyOff() const;

private:
   enum Kind : uint8_t
   {
      Held,        // OFF: until ON, a DTR drop or max_ms
      Cycling,     // CYCLE: until off_ms
      Releasing,   // a DTR drop ended the cut; on again after the minimum off time
   };
   struct Chan
   {
      bool     Off = false;
      Kind     K = Held;
      uint64_t OffAtUs = 0;         // when power was cut
      uint64_t RestoreAtUs = 0;     // when power comes back by itself
      uint32_t Ms = 0;              // the OFF's max_ms or the CYCLE's off_ms
      bool     CycleOwed = false;   // a CYCLE reply is owed
      bool     Early = false;       // ON or a DTR drop is ending the cut before its time
      uint8_t  OnOwed = 0;          // ON replies owed, sent once power is back
   };

   Hal            &H;
   const Identity &Id;
   uint8_t         Channels;
   Chan            Ch[MaxChannels];

   void Reply(const char *Line) { H.WriteLine(Line); }
   void Cut(uint8_t i, Kind K, uint32_t Ms, uint64_t Now);
   void Restore(uint8_t i);
   void EndEarly(uint8_t i, uint64_t Now);
   const char *PowerWord(uint8_t i) const;

   void CmdIdentify(int Argc);
   void CmdState(int Argc, const char *const *Arg, const int *Len, uint64_t Now);
   void CmdOn(int Argc, const char *const *Arg, const int *Len, uint64_t Now);
   void CmdOff(int Argc, const char *const *Arg, const int *Len, uint64_t Now);
   void CmdCycle(int Argc, const char *const *Arg, const int *Len, uint64_t Now);
   bool Channel(const char *Name, const char *Arg, int Len, uint8_t *Index);
   bool Millis(const char *Name, int Argc, const char *Arg, int Len, uint32_t Default, uint32_t Lo, uint32_t Hi,
               uint32_t *Ms);
};
