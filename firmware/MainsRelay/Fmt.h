/*
  Small text helpers for host protocol lines.  No Arduino dependencies:
  64-bit numbers are formatted here because printf's %llu is not available
  in every Teensy C library build.
*/
#pragma once
#include <stdint.h>

class LineBuf
{
public:
   static const int Cap = 200;   // the longest line, ? on a four-channel controller, is about 170

   LineBuf &Str(const char *s);
   LineBuf &Ch(char c);
   LineBuf &U64(uint64_t v);
   const char *C() { B[N] = 0; return B; }

private:
   char B[Cap + 1] = {};
   int  N = 0;
};

// Text[0..Len) is Word, ignoring ASCII case: command names
bool SameWord(const char *Text, int Len, const char *Word);

// Text[0..Len) as a decimal number: digits only, no sign, at most 4294967295
bool ParseU32(const char *Text, int Len, uint32_t *Value);

// __DATE__ ("Nov 16 2026") and __TIME__ ("14:41:00") to "2026-11-16T14:41:00".
// Returns false, with Out set to "unknown", if either is malformed.
bool FormatBuild(const char *Date, const char *Time, char *Out, int OutCap);
