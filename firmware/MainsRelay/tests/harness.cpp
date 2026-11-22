// Host harness for the controller's protocol engine.  Reads a script on stdin
// and prints what the controller would send ("< ...") and every relay change
// with its time ("= power <ch> on|off @µs").  Built and driven by
// host-protocol.test.mjs.
//
// Script lines:
//   boot <channels>      Engine.Begin with this many channels (first line)
//   > <text>             send a line (text plus LF)
//   >> <text>            send bytes with no LF
//   raw <hex> ...        send raw bytes
//   +<us>                advance the clock, polling every 1000 µs
//   dtr0, dtr1           the host closes the port, or opens it (it starts open)
//   build <date>|<time>  format a build stamp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../Engine.h"
#include "../HostPort.h"
#include "../Fmt.h"

class FakeHal : public Hal
{
public:
   uint64_t Now = 1000000;
   bool     Connected = true;

   uint64_t NowUs() override { return Now; }
   void SetPower(uint8_t Ch, bool On) override
   {
      printf("= power %u %s @%llu\n", (unsigned)Ch + 1, On ? "on" : "off", (unsigned long long)Now);
   }
   void WriteLine(const char *Line) override
   {
      if (Connected) printf("< %s\n", Line);
   }
};

static FakeHal  TheHal;
static Identity Id;
static Engine  *E = nullptr;
static HostPort *Port = nullptr;

static void Bytes(const std::string &s)
{
   for (unsigned char c : s) Port->Byte(c, TheHal.Now);
}

static void Advance(uint64_t Us)
{
   uint64_t end = TheHal.Now + Us;
   while (TheHal.Now < end)
   {
      uint64_t step = end - TheHal.Now < 1000 ? end - TheHal.Now : 1000;
      TheHal.Now += step;
      Port->Tick(TheHal.Now);
      E->Poll();
   }
}

int main()
{
   Id.Fw = "1.0.0";
   Id.Build = "2026-01-02T03:04:05";
   Id.Mcu = "teensy31";
   Id.Chip = 12345678;

   char line[512];
   while (fgets(line, sizeof line, stdin))
   {
      std::string s(line);
      while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
      if (s.empty()) continue;

      if (s.rfind("boot ", 0) == 0)
      {
         E = new Engine(TheHal, Id, (uint8_t)atoi(s.c_str() + 5));
         Port = new HostPort(*E);
         Port->SetConnected(true);
         E->Begin();
      }
      else if (s.rfind("build ", 0) == 0)
      {
         std::string arg = s.substr(6);
         size_t bar = arg.find('|');
         char out[32];
         bool ok = FormatBuild(arg.substr(0, bar).c_str(), arg.substr(bar + 1).c_str(), out, sizeof out);
         printf("build %s %s\n", ok ? "ok" : "bad", out);
      }
      else if (!E)
      {
         fprintf(stderr, "boot first: %s\n", s.c_str());
         return 2;
      }
      else if (s.rfind(">> ", 0) == 0) Bytes(s.substr(3));
      else if (s == ">" || s.rfind("> ", 0) == 0) Bytes((s.size() > 2 ? s.substr(2) : std::string()) + "\n");
      else if (s.rfind("raw", 0) == 0)
      {
         const char *p = s.c_str() + 3;
         char *end;
         for (;;)
         {
            long v = strtol(p, &end, 16);
            if (end == p) break;
            Port->Byte((uint8_t)v, TheHal.Now);
            p = end;
         }
      }
      else if (s[0] == '+') Advance(strtoull(s.c_str() + 1, nullptr, 10));
      else if (s == "dtr0" || s == "dtr1")
      {
         TheHal.Connected = s == "dtr1";
         Port->SetConnected(TheHal.Connected);
      }
      else
      {
         fprintf(stderr, "unknown script line: %s\n", s.c_str());
         return 2;
      }
   }
   return 0;
}
