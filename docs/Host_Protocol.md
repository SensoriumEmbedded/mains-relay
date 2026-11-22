# Host Protocol

This document describes the text command protocol on the controller's USB serial port. A host program ([TReadmill](https://github.com/SensoriumEmbedded/treadmill), a terminal, a script) uses it to cut and restore a machine's mains power and to run a timed power cycle. It is the contract: the firmware and every host are tested against it.

Protocol version: **1** (`proto=1` in the `?` reply).

## Connection

- The controller is a USB CDC serial device on the Teensy's micro-USB port. The baud rate is ignored, with one exception: never open the port at 134 baud, which reboots a Teensy into its bootloader. In the bootloader the relay is off (see "Power behavior").
- Its USB serial string is `MainsRelay_<chipid>`, with the chip id in decimal. The separator is an underscore because pyserial on Windows drops a serial string that contains a dash. The product string is `Mains Relay`. A host finds the controller by its serial string without opening any port. The chip id is the controller's stable unit id.
- The host must assert DTR, which pyserial and most terminals do by default. When DTR drops (the host closes the port) or the USB bus suspends (the cable is pulled or the PC sleeps), the controller releases everything the host holds (see "Power behavior"). Both count as a DTR drop below.
- Bytes that arrive while DTR is low, or while the bus is suspended, are discarded, never run.
- The controller is powered by USB only, never from the supply it switches.

## Lines

- From host to controller: printable ASCII (0x20 to 0x7E), one command per line, ending in LF. CR is ignored. A line is at most 64 characters, not counting the LF.
- A line longer than 64 characters, or one holding any other byte, is dropped whole and answered with `ERR * syntax` when its LF arrives.
- A partial line is discarded with no reply after 500 ms with no new byte, or when DTR drops. TeensyROM tools send the bytes `dv` with no LF to every Teensy port they find, and this rule drops them.
- An empty line is ignored with no reply. A host can send a bare LF to resync before `?`.
- Fields are separated by one or more spaces. Command names are case-insensitive.

## Replies

- Every command line gets exactly one reply line: `OK <CMD> [fields]` or `ERR <CMD> <reason>`. Lines from the controller end in LF.
- `<CMD>` is the command name in upper case (`?`, `STATE`, `ON`, `OFF`, `CYCLE`), or `*` when the line has no recognizable command.
- Reasons:

  | Reason | Meaning |
  |--------|---------|
  | `syntax` | Unknown command, a missing channel, or a bad number |
  | `range` | A channel this controller doesn't have, or a time outside its range |
  | `busy` | `OFF` or `CYCLE` sent to a channel in a `CYCLE` or with an `ON` waiting out the minimum off time, or `CYCLE` sent to a channel that is off |
  | `aborted` | `ON` cut a `CYCLE` short |

- Fields are `key=value`, with no spaces in values. Hosts ignore fields they don't know, so new fields can be added without a new protocol version.
- A reply is sent when its command's action has finished. `CYCLE` replies once power is back, and an `ON` sent within the minimum off time replies once that time has passed. Every other command replies at once, so replies can arrive in a different order from the commands. A host matches a reply to the oldest pending command of its name.

## Events

- Lines the controller sends on its own start with `EV `. They are never written in the middle of a reply line. Hosts ignore `EV` lines they don't know.
- `EV OFF <ch> timeout` means an `OFF` reached its `max_ms`, and power came back on.

## Channels

Channels are numbered from 1, in the order `Pins.h` lists them. `ch=` in the `?` reply gives the count. Every command that switches power names its channel, even on a one-channel controller.

## Commands

| Command | Action | Range | Default |
|---------|--------|-------|---------|
| `?` | Identity and state | | |
| `STATE <ch>` | One channel's state | | |
| `ON <ch>` | Restore power. A running `CYCLE` on the channel ends: it replies `ERR CYCLE aborted`, then `ON` replies | | |
| `OFF <ch> [max_ms]` | Cut power and hold it off until `ON`, a DTR drop or `max_ms`. `OFF` on a channel already held off starts a new `max_ms` from now | max_ms 2000 to 3600000 | 600000 |
| `CYCLE <ch> [off_ms]` | Cut power, wait `off_ms`, restore it | off_ms 2000 to 60000 | 5000 |

### Replies

| Command | Reply |
|---------|-------|
| `?` | `OK ? proto=1 name=MainsRelay fw=<x.y.z> build=<yyyy-mm-ddThh:mm:ss> chip=<decimal> mcu=<teensy31\|…> ch=<n> state=<list> min_off_ms=<n> max_off_ms=<n>` |
| `STATE` | `OK STATE ch=<n> power=<on\|off\|cycling> left_ms=<n>` |
| `ON` | `OK ON ch=<n> power=on` |
| `OFF` | `OK OFF ch=<n> power=off max_ms=<n>` |
| `CYCLE` | `OK CYCLE ch=<n> off_ms=<n>` |

`build` is the newest compile time among the firmware's source files. `state` lists each channel's power in channel order, comma-separated (`state=on`, `state=on,off`). `left_ms` is the time until power returns by itself: the rest of a `CYCLE`'s `off_ms`, or of an `OFF`'s `max_ms`. It is 0 for a channel that is on.

## Power behavior

- **Minimum off time.** Once a channel cuts power, it stays off for at least `min_off_ms` (2000) whatever asks, including a DTR drop. A power glitch shorter than that is worse for the machine than a clean cycle.
- **The controller times every cut.** A `CYCLE` ends when the controller's clock says so, and an `OFF` ends at its `max_ms` at the latest. A host that stops sending, hangs or exits can never leave a channel off for longer than that.
- **A DTR drop is `ON` for every channel**, after the minimum off time. A running `CYCLE` ends early and sends no reply. Nothing from one session reaches the next one. A new session that opens before power is back sees the channel as `off`, and its `OFF` holds it again.
- **While the firmware runs, a channel is on** unless an `OFF` or `CYCLE` holds it off. At reset the firmware drives every channel on before USB starts, within a few milliseconds.
- **The relay is off when the firmware isn't running.** This covers a controller with no USB power, one being reflashed, and one in its bootloader. The supply's switch, wired across the relay, bypasses it: on ON the machine has power whatever the controller does, and on AUTO the controller decides.
- A hung controller is reset by its watchdog and comes back on as above.

The controller cannot tell whether the bypass switch is on, or whether anything is drawing current. A host learns both by watching the machine: [TReadmill](https://github.com/SensoriumEmbedded/treadmill) fails a power cycle in which the TeensyROM never left USB.

## Example

```
> ?
< OK ? proto=1 name=MainsRelay fw=1.0.0 build=2026-11-23T10:12:44 chip=12345678 mcu=teensy31 ch=1 state=on min_off_ms=2000 max_off_ms=600000
> CYCLE 1
< OK CYCLE ch=1 off_ms=5000
> OFF 1 60000
< OK OFF ch=1 power=off max_ms=60000
> STATE 1
< OK STATE ch=1 power=off left_ms=52310
> CYCLE 1
< ERR CYCLE busy
> ON 1
< OK ON ch=1 power=on
> ON 2
< ERR ON range
> CYCLE 1 500
< ERR CYCLE range
```

## Versioning

New commands, fields, `EV` kinds and names keep `proto=1`. A change that would break an existing host raises the number, and a host refuses a `proto` it doesn't speak.
