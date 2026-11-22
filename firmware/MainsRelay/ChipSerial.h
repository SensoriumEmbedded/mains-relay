/*
  This chip's unique number, as PJRC's core computes it for the default USB
  serial number (usb_init_serialnumber() in cores/teensy3/usb_desc.c): the
  flash's program-once record 15 on the MK20DX256 of a Teensy 3.1/3.2, times
  10 when it's under 8 digits.  It is the controller's unit id.
  static inline: included from C (UsbNames.c) and C++ alike.
*/
#pragma once
#include <stdint.h>

#if defined(__MK20DX256__)
#include "kinetis.h"
static inline uint32_t MR_ChipSerialNum(void)
{
   uint32_t primask, serialNum;
   __asm__ volatile("mrs %0, primask" : "=r"(primask));
   __disable_irq();
   FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
   FTFL_FCCOB0 = 0x41;   // read once
   FTFL_FCCOB1 = 15;     // record 15
   FTFL_FSTAT = FTFL_FSTAT_CCIF;
   while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ;
   serialNum = *(uint32_t *)&FTFL_FCCOB7;
   if (!(primask & 1)) __enable_irq();
   if (serialNum < 10000000) serialNum *= 10;
   return serialNum;
}
#define MR_MCU "teensy31"
#else
static inline uint32_t MR_ChipSerialNum(void) { return 0; }
#define MR_MCU "teensy"
#endif
