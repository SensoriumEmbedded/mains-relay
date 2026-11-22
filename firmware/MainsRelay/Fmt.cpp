#include "Fmt.h"
#include "BuildStamp.h"
#include <string.h>

static char Lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

bool SameWord(const char *Text, int Len, const char *Word)
{
   int i = 0;
   for (; i < Len; i++)
   {
      if (!Word[i] || Lower(Text[i]) != Lower(Word[i])) return false;
   }
   return Word[i] == 0;
}

static bool Digit(char c) { return c >= '0' && c <= '9'; }

bool ParseU32(const char *Text, int Len, uint32_t *Value)
{
   if (Len < 1 || Len > 10) return false;
   uint64_t v = 0;
   for (int i = 0; i < Len; i++)
   {
      if (!Digit(Text[i])) return false;
      v = v * 10 + (uint64_t)(Text[i] - '0');
   }
   if (v > 0xFFFFFFFFu) return false;
   *Value = (uint32_t)v;
   return true;
}

LineBuf &LineBuf::Ch(char c)
{
   if (N < Cap) B[N++] = c;
   return *this;
}

LineBuf &LineBuf::Str(const char *s)
{
   while (*s) Ch(*s++);
   return *this;
}

LineBuf &LineBuf::U64(uint64_t v)
{
   char d[21];
   int i = 0;
   do { d[i++] = (char)('0' + v % 10); v /= 10; } while (v);
   while (i) Ch(d[--i]);
   return *this;
}

bool FormatBuild(const char *Date, const char *Time, char *Out, int OutCap)
{
   static const char Months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
   if (OutCap < 20) { if (OutCap > 0) Out[0] = 0; return false; }
   // Date: "Mmm dd yyyy" with the day space-padded; Time: "hh:mm:ss"
   bool ok = Date && Time && strlen(Date) == 11 && strlen(Time) == 8 &&
             (Date[4] == ' ' || Digit(Date[4])) && Digit(Date[5]) &&
             Digit(Date[7]) && Digit(Date[8]) && Digit(Date[9]) && Digit(Date[10]) &&
             Digit(Time[0]) && Digit(Time[1]) && Time[2] == ':' &&
             Digit(Time[3]) && Digit(Time[4]) && Time[5] == ':' &&
             Digit(Time[6]) && Digit(Time[7]);
   int month = 0;
   if (ok)
   {
      for (int m = 0; m < 12; m++)
      {
         if (strncmp(Date, Months + m * 3, 3) == 0) { month = m + 1; break; }
      }
      ok = month != 0;
   }
   if (!ok)
   {
      strncpy(Out, "unknown", (size_t)OutCap);
      Out[OutCap - 1] = 0;
      return false;
   }
   char *o = Out;
   memcpy(o, Date + 7, 4); o += 4;
   *o++ = '-';
   *o++ = (char)('0' + month / 10);
   *o++ = (char)('0' + month % 10);
   *o++ = '-';
   *o++ = Date[4] == ' ' ? '0' : Date[4];
   *o++ = Date[5];
   *o++ = 'T';
   memcpy(o, Time, 8); o += 8;
   *o = 0;
   return true;
}
