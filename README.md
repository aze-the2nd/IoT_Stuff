# IoT Stuff — Smart RTD Temperature Sensor (early exploration)

Status: **early-stage, hardware not yet decided.** This repo is a scaffold for
capturing context and open questions before any implementation work starts.
Nothing here should be read as a committed architecture.

## Goal

Build a "smart sensor" that reads temperature from a Pt-100 or Pt-1000 RTD
(resistance temperature detector) and exposes/displays it.

## Background (so it doesn't need re-deriving later)

- RTDs are passive resistive elements — Pt-100 is ~100Ω at 0°C
  (~0.385Ω/°C), Pt-1000 is ~1000Ω at 0°C (~3.85Ω/°C). They need signal
  conditioning; a plain ADC pin can't read them directly.
- The standard, easy, accurate approach is a dedicated RTD-to-digital IC:
  the **MAX31865** (SPI interface, supports 2/3/4-wire RTD wiring, built-in
  reference resistor, linearization, fault detection). Commonly available as
  an Adafruit breakout with solder-jumper wire-count selection.
- **Pt-1000 vs Pt-100:** Pt-1000 is friendlier for cheaper/DIY signal
  chains — 10x more resistance/signal for the same excitation current, less
  sensitive to lead resistance. Pt-100 is more common industrially.
- **Wiring:** 4-wire (Kelvin) wiring eliminates lead-resistance error
  entirely — two wires carry excitation current, two separate
  high-impedance wires sense voltage right at the sensor. The MAX31865
  supports 4-wire directly via its FORCE+/FORCE-/RTDIN+/RTDIN- pins. A DIY
  (no dedicated IC) 4-wire approach would need an actual constant-current
  source plus a differential measurement (e.g. an instrumentation amp like
  the INA333) — a lot more complex than just using the MAX31865.

## Hardware — NOT yet decided (two live candidates)

1. **Raspberry Pi Pico** (RP2040/RP2350) + a MAX31865 breakout over SPI.
   Would need a separate display if local readout is wanted — the Pico has
   no built-in screen.
2. **ESP32-2432S028** ("Cheap Yellow Display" / CYD) — an ESP32 dev board
   with a built-in 2.8" TFT (often with resistive touch). Attractive
   because it could host both the RTD frontend (still likely a MAX31865
   over SPI, sharing/multiplexing the bus with the built-in display) *and*
   show the temperature directly on-screen — no separate display needed.
   WiFi-capable, which may matter if "smart sensor" implies network/IoT
   connectivity (MQTT, a web UI, Home Assistant integration, etc. — not yet
   discussed with the user).

Both boards are plausible. The RTD signal-conditioning approach
(MAX31865, Pt-100 vs Pt-1000) is likely independent of which
microcontroller wins, since the MAX31865 talks generic SPI to either.

## Open questions to resolve before committing to an implementation

- **Board:** Pico + MAX31865 + separate display, vs. ESP32-2432S028
  all-in-one?
- **RTD type:** Pt-100 or Pt-1000?
- **Wiring:** 2-wire, 3-wire, or 4-wire RTD connection?
- **"Smart" scope:** Does this imply WiFi/network reporting? If so, what
  protocol/target — MQTT, a local web UI, Home Assistant integration,
  something else?
- **Power source:** battery, USB, mains-adjacent supply?
- **Enclosure:** any physical packaging/mounting constraints (e.g. probe
  form factor, IP rating for the environment it's measuring)?

## Status

No firmware or final architecture yet — this is a scaffold-and-capture-context
step. Resume once the hardware direction above is narrowed down.
