# The Annoyer

A maker-workshop sound device built around an Adafruit Trinket M0 and a DFR0299 (DFPlayer Mini), designed and soldered by kids at the **Omnisight Job Shadow Day 2026** in partnership with University School of the Lowcountry.

Two buttons, five modes:

- **Sound Machine** — loop a calming sound (rain, ocean, white noise) for 60 minutes, then sleep
- **Noise Maker** — soundboard for effects
- **Quotes** — soundboard for movie/motivational quotes
- **Daily Annoyer** — fire a sound effect every 24 hours (random or chosen) until the battery dies
- **Hourly Annoyer** — same idea, every hour

## Files

- [`Annoyer.ino`](Annoyer.ino) — Arduino sketch for the Trinket M0
- [`generate_sounds.sh`](generate_sounds.sh) — regenerates the menu narration clips via ElevenLabs (needs an API key)
- [`copy_to_sd.sh`](copy_to_sd.sh) — copies sound files onto an SD card in numerical order so the DFPlayer plays them in prefix order
- [`HOW_TO_USE.md`](HOW_TO_USE.md) / [`HOW_TO_USE.pdf`](HOW_TO_USE.pdf) — printable instructions for the kids
- `sounds/01/` — system narration clips (committed)
- `sounds/02/`, `sounds/03/`, `sounds/07/` — content folders (your sounds go here, not committed)

## Hardware

- Adafruit Trinket M0 (SAMD21)
- DFR0299 MP3 player (DFPlayer Mini)
- Two non-latching push buttons (D0 = Play/OK, D1 = Next)
- Transistor on D2 for switching DFR0299 power for deep-sleep modes
- 2000 mAh battery

## Quick start

1. Install **Adafruit SAMD Boards** in Arduino IDE (Boards Manager)
2. Install the libraries:
   - DFRobotDFPlayerMini
   - ArduinoLowPower
   - RTCZero
   - FlashStorage
3. Flash `Annoyer.ino` to a Trinket M0
4. Prepare an SD card: format FAT32, copy the contents of `sounds/` onto it using `./copy_to_sd.sh`
5. Pop in the card, power up, listen for "Select mode"

Full instructions and a printable handout are in [`HOW_TO_USE.md`](HOW_TO_USE.md).
