# Hardware

> **PLACEHOLDER: to be supplied.** The schematic, BOM, wiring details and photos aren't written yet. Nothing below names a part that hasn't been chosen.

## Known so far

| Item | What |
|---|---|
| Controller | Teensy 3.1, powered from USB only |
| Relay | Opto 22 240D45 solid-state relay: DC input (3 V pick-up, 1 V drop-out, 1,000 Ω nominal), zero-voltage turn-on, 24–280 VAC load |
| Drive | Straight from one Teensy pin to the relay's input. The relay's input pulls a floating pin low, so the relay is off |
| Mains | 120 V. The relay's load terminals sit across the supply's single-pole power switch, which switches the hot (black) wire, ahead of the supply's fuse: inlet → switch ‖ relay → fuse → supply |
| Bypass | The supply's own switch: ON is always on, its off position is AUTO |
| Enclosure | The supply's case. Only the USB cable leaves it |
| Supply | A switching supply for 5 VDC and a transformer for 9 VAC |

## To be supplied

- [ ] Schematic (`schematic/`)
- [ ] BOM ([bom.md](bom.md))
- [ ] Wiring: which Teensy pin, wire gauge, how the relay is mounted, how the low-voltage side is kept clear of mains ([wiring.md](wiring.md))
- [ ] Photos (`photos/`)
- [ ] Whether a panel button is wired to the Teensy's Program pad (otherwise an image that breaks USB means opening the case)
