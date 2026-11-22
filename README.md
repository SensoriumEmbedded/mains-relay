# Mains Relay

A USB-controlled mains switch for a bench power supply. A Teensy 3.1 drives a solid-state relay, and a host on the bench PC cuts and restores the supply's mains over USB serial. It was built so that TReadmill, the hardware-in-the-loop suite for TeensyROM firmware, can power-cycle a C64 or C128 that no reset brings back. Nothing in it is specific to TReadmill.

**Status:** the protocol and the firmware are written, and the firmware passes its host tests. Neither has run on the hardware yet, and the hardware notes are still to come.

## How it is wired

The relay and the Teensy sit inside the supply's case, and a USB cable is the only thing that leaves it. The relay's load terminals are wired across the supply's own single-pole power switch, on the hot side, ahead of the supply's fuse. That makes the switch the bypass:

| Supply switch | Machine power |
|---|---|
| **ON** | Always on, whatever the controller does |
| **AUTO** (the switch's off position) | Decided by the controller: on while its firmware runs, off while a host holds it off or runs a power cycle |

On AUTO the supply's switch no longer turns the machine off while the controller has USB power. To turn the machine off for a cartridge or cable swap, use the machine's own switch or pull the plug.

The relay is off whenever the controller's firmware isn't running. That covers no USB power, a reflash, and the bootloader. Switch to ON for manual use and while reflashing the controller.

[hardware/](hardware/README.md) will hold the schematic, BOM, wiring and photos.

## Talking to it

Any serial terminal works. Find the port whose USB serial string is `MainsRelay_<chip id>`, open it at any baud rate except 134, and type:

```
?
CYCLE 1
```

`?` identifies the controller and reports each channel's state. `CYCLE 1` cuts channel 1 for 5 seconds, then replies once power is back. [docs/Host_Protocol.md](docs/Host_Protocol.md) is the full contract: every command, the replies, and what happens to the power when the host goes away.

## Building and testing

The sketch is `firmware/MainsRelay/`, for a Teensy 3.1 or 3.2 with Teensyduino 1.61:

```
arduino-cli compile --fqbn teensy:avr:teensy31 firmware/MainsRelay
```

Set `RelayPins` in `Pins.h` to the pin that's wired; pin 2 is a placeholder. Switch the supply to ON before you reflash: the relay is off while the Teensy is in its bootloader.

The protocol tests run the firmware's own engine on the PC, with a simulated clock and relay. They need Node.js and a `c++` compiler on `PATH`:

```
node --test "firmware/MainsRelay/tests/*.test.mjs"
```

## Safety

- The supply's case holds mains wiring. Work on it unplugged.
- The Teensy takes power from USB only, and connects to nothing in the case except the relay's control input, which is isolated from the mains side. Powered from the supply it switches, a power cycle would cut the controller's own power, and the machine would never come back.
- Keep the Teensy and its wiring clear of the mains side, and strain-relieve the USB cable where it leaves the case.
- Nothing a host does can leave the machine off for long while the firmware runs. A power cycle ends on the controller's clock, a hold-off ends at its limit (10 minutes by default), and a host that closes the port or sleeps gets the power back on. See "Power behavior" in the protocol.

## Layout

| Path | What |
|---|---|
| `docs/Host_Protocol.md` | The USB serial protocol (proto 1) |
| `firmware/MainsRelay/` | The Teensy sketch; `tests/` runs its protocol engine on the PC |
| `hardware/` | Schematic, BOM, wiring, photos (to be supplied) |

## License

MIT; see [LICENSE](LICENSE).
