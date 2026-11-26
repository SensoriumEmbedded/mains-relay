#include "Engine.h"
#include "BuildStamp.h"
#include "Fmt.h"

static const int MaxArgs = 4;

Engine::Engine(Hal &H_, const Identity &Id_, uint8_t Channels_)
   : H(H_), Id(Id_), Channels(Channels_ > MaxChannels ? MaxChannels : Channels_)
{
}

void Engine::Begin()
{
   for (uint8_t i = 0; i < Channels; i++)
   {
      Ch[i] = Chan();
      H.SetPower(i, true);
   }
}

bool Engine::AnyOff() const
{
   for (uint8_t i = 0; i < Channels; i++)
   {
      if (Ch[i].Off) return true;
   }
   return false;
}

const char *Engine::PowerWord(uint8_t i) const
{
   if (!Ch[i].Off) return "on";
   return Ch[i].K == Cycling ? "cycling" : "off";
}

void Engine::Cut(uint8_t i, Kind K, uint32_t Ms, uint64_t Now)
{
   Chan &c = Ch[i];
   c = Chan();
   c.Off = true;
   c.K = K;
   c.OffAtUs = Now;
   c.RestoreAtUs = Now + (uint64_t)Ms * 1000;
   c.Ms = Ms;
   H.SetPower(i, false);
}

// Brings forward the end of a cut, never to less than the minimum off time
void Engine::EndEarly(uint8_t i, uint64_t Now)
{
   Chan &c = Ch[i];
   uint64_t earliest = c.OffAtUs + (uint64_t)MinOffMs * 1000;
   uint64_t at = Now > earliest ? Now : earliest;
   if (at < c.RestoreAtUs) c.RestoreAtUs = at;
   c.Early = true;
}

void Engine::Restore(uint8_t i)
{
   Chan c = Ch[i];
   Ch[i] = Chan();
   H.SetPower(i, true);
   LineBuf b;
   if (c.CycleOwed)
   {
      if (c.Early) Reply("ERR CYCLE aborted");
      else Reply(b.Str("OK CYCLE ch=").U64(i + 1).Str(" off_ms=").U64(c.Ms).C());
   }
   for (uint8_t n = 0; n < c.OnOwed; n++)
   {
      LineBuf on;
      Reply(on.Str("OK ON ch=").U64(i + 1).Str(" power=on").C());
   }
   if (c.K == Held && !c.Early)
   {
      LineBuf ev;
      Reply(ev.Str("EV OFF ").U64(i + 1).Str(" timeout").C());
   }
}

void Engine::Poll()
{
   uint64_t now = H.NowUs();
   for (uint8_t i = 0; i < Channels; i++)
   {
      if (Ch[i].Off && now >= Ch[i].RestoreAtUs) Restore(i);
   }
}

void Engine::OnDtrDrop()
{
   uint64_t now = H.NowUs();
   for (uint8_t i = 0; i < Channels; i++)
   {
      Chan &c = Ch[i];
      if (!c.Off) continue;
      if (c.K == Latched)
      {
         c.OnOwed = 0;   // a latch stays (or an ON already ending it goes on), but its reply belonged to that session
         continue;
      }
      EndEarly(i, now);
      c.K = Releasing;
      c.CycleOwed = false;
      c.OnOwed = 0;
   }
   Poll();
}

void Engine::OnJunkLine()
{
   Reply("ERR * syntax");
}

void Engine::OnLine(const char *Text)
{
   Poll();   // settle what is due first, so a reply never reports a cut that has already ended
   uint64_t now = H.NowUs();

   const char *arg[MaxArgs + 1] = {};
   int len[MaxArgs + 1] = {};
   int argc = 0;
   const char *p = Text;
   while (*p)
   {
      while (*p == ' ') p++;
      if (!*p) break;
      const char *start = p;
      while (*p && *p != ' ') p++;
      if (argc <= MaxArgs)
      {
         arg[argc] = start;
         len[argc] = (int)(p - start);
      }
      argc++;
   }
   if (argc == 0) return;
   if (argc > MaxArgs + 1) argc = MaxArgs + 1;   // more words than any command takes: a syntax error below

   const char *const *a = arg + 1;
   const int *l = len + 1;
   int n = argc - 1;
   if (SameWord(arg[0], len[0], "?")) CmdIdentify(n);
   else if (SameWord(arg[0], len[0], "STATE")) CmdState(n, a, l, now);
   else if (SameWord(arg[0], len[0], "ON")) CmdOn(n, a, l, now);
   else if (SameWord(arg[0], len[0], "OFF")) CmdOff(n, a, l, now);
   else if (SameWord(arg[0], len[0], "CYCLE")) CmdCycle(n, a, l, now);
   else Reply("ERR * syntax");
}

bool Engine::Channel(const char *Name, const char *Arg, int Len, uint8_t *Index)
{
   uint32_t v;
   LineBuf b;
   if (!ParseU32(Arg, Len, &v))
   {
      Reply(b.Str("ERR ").Str(Name).Str(" syntax").C());
      return false;
   }
   if (v < 1 || v > Channels)
   {
      Reply(b.Str("ERR ").Str(Name).Str(" range").C());
      return false;
   }
   *Index = (uint8_t)(v - 1);
   return true;
}

bool Engine::Millis(const char *Name, int Argc, const char *Arg, int Len, uint32_t Default, uint32_t Lo, uint32_t Hi,
                    uint32_t *Ms)
{
   if (Argc < 2)
   {
      *Ms = Default;
      return true;
   }
   LineBuf b;
   if (!ParseU32(Arg, Len, Ms))
   {
      Reply(b.Str("ERR ").Str(Name).Str(" syntax").C());
      return false;
   }
   if (*Ms < Lo || *Ms > Hi)
   {
      Reply(b.Str("ERR ").Str(Name).Str(" range").C());
      return false;
   }
   return true;
}

void Engine::CmdIdentify(int Argc)
{
   if (Argc != 0)
   {
      Reply("ERR ? syntax");
      return;
   }
   LineBuf b;
   b.Str("OK ? proto=1 name=MainsRelay fw=").Str(Id.Fw).Str(" build=").Str(Id.Build)
    .Str(" chip=").U64(Id.Chip).Str(" mcu=").Str(Id.Mcu).Str(" ch=").U64(Channels).Str(" state=");
   for (uint8_t i = 0; i < Channels; i++)
   {
      if (i) b.Ch(',');
      b.Str(PowerWord(i));
   }
   b.Str(" min_off_ms=").U64(MinOffMs).Str(" max_off_ms=").U64(OffDefaultMs);
   Reply(b.C());
}

void Engine::CmdState(int Argc, const char *const *Arg, const int *Len, uint64_t Now)
{
   uint8_t i;
   if (Argc != 1)
   {
      Reply("ERR STATE syntax");
      return;
   }
   if (!Channel("STATE", Arg[0], Len[0], &i)) return;
   const Chan &c = Ch[i];
   uint64_t left = c.Off && c.RestoreAtUs > Now ? (c.RestoreAtUs - Now + 999) / 1000 : 0;
   if (c.Off && c.K == Latched && !c.Early) left = 0;   // never comes back by itself
   LineBuf b;
   b.Str("OK STATE ch=").U64(i + 1).Str(" power=").Str(PowerWord(i)).Str(" left_ms=").U64(left);
   if (c.Off && c.K == Latched) b.Str(" latch=1");
   Reply(b.C());
}

void Engine::CmdOn(int Argc, const char *const *Arg, const int *Len, uint64_t Now)
{
   uint8_t i;
   if (Argc != 1)
   {
      Reply("ERR ON syntax");
      return;
   }
   if (!Channel("ON", Arg[0], Len[0], &i)) return;
   Chan &c = Ch[i];
   if (!c.Off)
   {
      LineBuf b;
      Reply(b.Str("OK ON ch=").U64(i + 1).Str(" power=on").C());
      return;
   }
   if (c.OnOwed < 255) c.OnOwed++;
   EndEarly(i, Now);
   Poll();
}

void Engine::CmdOff(int Argc, const char *const *Arg, const int *Len, uint64_t Now)
{
   uint8_t i;
   uint32_t ms = 0;
   if (Argc < 1 || Argc > 2)
   {
      Reply("ERR OFF syntax");
      return;
   }
   if (!Channel("OFF", Arg[0], Len[0], &i)) return;
   bool latch = Argc == 2 && SameWord(Arg[1], Len[1], "LATCH");
   if (!latch && !Millis("OFF", Argc, Arg[1], Len[1], OffDefaultMs, MinOffMs, OffMaxMs, &ms)) return;
   Chan &c = Ch[i];
   if (c.Off && (c.K == Cycling || c.OnOwed))
   {
      Reply("ERR OFF busy");
      return;
   }
   if (c.Off && c.K == Latched)
   {
      latch = true;   // only ON ends a latch; a plain OFF leaves it latched
      ms = 0;
   }
   if (!c.Off) Cut(i, Held, 0, Now);
   // Already off (held, latched, or releasing after a DTR drop): hold it again, with no glitch
   c.Early = false;
   c.K = latch ? Latched : Held;
   c.RestoreAtUs = latch ? UINT64_MAX : Now + (uint64_t)ms * 1000;
   c.Ms = ms;
   LineBuf b;
   b.Str("OK OFF ch=").U64(i + 1).Str(" power=off max_ms=").U64(ms);
   if (latch) b.Str(" latch=1");
   Reply(b.C());
}

void Engine::CmdCycle(int Argc, const char *const *Arg, const int *Len, uint64_t Now)
{
   uint8_t i;
   uint32_t ms;
   if (Argc < 1 || Argc > 2)
   {
      Reply("ERR CYCLE syntax");
      return;
   }
   if (!Channel("CYCLE", Arg[0], Len[0], &i)) return;
   if (!Millis("CYCLE", Argc, Arg[1], Len[1], CycleDefaultMs, MinOffMs, CycleMaxMs, &ms)) return;
   if (Ch[i].Off)
   {
      Reply("ERR CYCLE busy");
      return;
   }
   Cut(i, Cycling, ms, Now);
   Ch[i].CycleOwed = true;
}
