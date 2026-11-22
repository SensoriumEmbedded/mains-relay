/*
  USB product and serial number strings, replacing the core's weak defaults
  ("USB Serial" and a bare number), so a host finds the controller by its
  serial string without opening any port: MainsRelay_<chip id>.  Letters,
  digits and the underscore only: pyserial on Windows takes a non-composite
  device's serial from its instance id and drops it when it has anything
  else (a '-').  Adapted from the C64-USBKeyboard sketch (same author).

  C only: usb_string_descriptor_struct ends in a flexible array member, and
  these initializers are valid C99 but not reliably accepted by g++.
*/
#if defined(__MK20DX256__)

#include "usb_names.h"
#include "BuildStamp.h"
#include "avr_functions.h"
#include "ChipSerial.h"

// Lengths must match the characters in each name.  The serial number's
// trailing 10 zeros reserve room for the chip id (up to 10 decimal digits).
#define PRODUCT_NAME     {'M','a','i','n','s',' ','R','e','l','a','y'}
#define PRODUCT_NAME_LEN 11

#define SERIAL_BASE_LEN  11   // "MainsRelay_"
#define SERIAL_NAME      {'M','a','i','n','s','R','e','l','a','y','_','0','0','0','0','0','0','0','0','0','0'}
#define SERIAL_NAME_LEN  21

// Do not change this part.  This exact format is required by USB.
struct usb_string_descriptor_struct usb_string_product_name = {
        2 + PRODUCT_NAME_LEN * 2,
        3,
        PRODUCT_NAME
};

struct usb_string_descriptor_struct usb_string_serial_number = {
        2 + SERIAL_NAME_LEN * 2,
        3,
        SERIAL_NAME
};

// Writes the chip id into the serial number's reserved digits and shrinks
// bLength to fit.  Runs from the boot hook (BootHook.cpp), before usb_init().
void UsbNames_AppendChipId(void)
{
	char buf[11];
	uint32_t i;

	ultoa(MR_ChipSerialNum(), buf, 10);
	for (i = 0; buf[i] && i < 10; i++) usb_string_serial_number.wString[SERIAL_BASE_LEN + i] = buf[i];
	usb_string_serial_number.bLength = 2 + (SERIAL_BASE_LEN + i) * 2;
}

#endif
