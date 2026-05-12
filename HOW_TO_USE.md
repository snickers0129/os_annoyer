# The Annoyer

**Omnisight Job Shadow Day 2026**
**University School of the Lowcountry**

You designed it. You soldered it. You programmed it. Now here's how to drive it.

---

## Your Two Buttons

| Button | What it does |
| --- | --- |
| **Play / OK** | Confirms a choice. Plays the current sound. |
| **Next** | Cycles to the next option. |

That's it. Two buttons do everything.

---

## Turning It On

Flip the power on. You'll hear:

> "Select mode."

Then it announces the first mode. From here:

- Press **Next** to hear the next mode.
- Press **Play / OK** to pick the one playing now.

---

## The Five Modes

### 1. Sound Machine
Plays a calming sound (rain, ocean, white noise) on a **seamless loop for 60 minutes**, then puts the device to sleep. Use it to fall asleep to.

### 2. Noise Maker
A button-driven soundboard for sound effects.
- **Next** plays a different effect each time.
- **Play / OK** replays the one you just heard.

### 3. Quotes
Same as Noise Maker, but the sounds are famous quotes.

### 4. Daily Annoyer
Pick a sound (or pick **"Random Sound"** for variety). The device sleeps for **24 hours**, wakes up just long enough to play the sound, then sleeps again. Forever. Or until the batteries die.

### 5. Hourly Annoyer
Same idea — but every **hour**. Use with extreme care.

---

## Picking a Sound

For Sound Machine, Daily Annoyer, and Hourly Annoyer you'll get a sound picker after choosing the mode.

1. You'll hear **"Select sound."**
2. The first option starts playing automatically.
3. Press **Next** to skip to the next sound.
4. Press **Play / OK** to lock in the one you're listening to.

**Pro tip — for both Annoyer modes, the FIRST option is "Random Sound."** Pick that, and the device chooses a different effect at every single wake-up. Maximum chaos.

---

## Adjusting the Volume

You can change volume from **any mode** at **any time**:

1. Press **BOTH buttons** at the same time. You'll hear "Adjust volume."
2. Press **Next** to make it **louder** — you'll hear "Up" at the new level.
3. Press **Play / OK** to make it **quieter** — you'll hear "Down" at the new level.
4. Stop pressing for 10 seconds. You'll hear "New volume saved." Done.

The volume sticks even after you turn the device off and on.

---

## During the Annoyer Modes

The Annoyer modes are different from the others — the device is asleep most of the time to save battery. A few things to know:

- **You can wake it up early.** Press **Next** while it's sleeping and it'll fire the annoy sound right then, then go back to its schedule.
- **To leave Annoyer mode**, unplug the device and plug it back in. You'll be returned to the main menu.

---

## After Sound Machine Finishes

After the 60-minute timer runs out, Sound Machine puts the whole device into deep sleep. To use it again, unplug and replug the battery.

---

## Adding Your Own Sounds

You can drop any MP3 you want into the right folder on the SD card and the device will pick it up. Here's how to do it without breaking anything.

### Which folder goes with which mode

| Folder on the card | What goes in it |
| --- | --- |
| `/02/` | Sound Machine loops (long, calming sounds — rain, ocean, white noise) |
| `/03/` | Noise Maker effects, AND the pool the Annoyer picks from |
| `/07/` | Quotes (movie lines, motivational stuff, whatever) |

The `/01/` folder is the menu narration. We already set it up — leave it alone unless you know what you're doing.

### Naming the files

Every file needs a **3-digit prefix followed by an underscore**. Examples:

- `001_rain.mp3`
- `002_white_noise.mp3`
- `076_my_new_meme.mp3`

Rules:
- Numbers must be **sequential with no gaps** (1, 2, 3, 4, … no skipping)
- Always pad to **three digits** (use `001`, not `1`)
- The text after the underscore is for you — the player only reads the number

### Why the order matters (the weird part)

The DFPlayer chip inside doesn't play files based on the number you wrote. It plays them **in the order they were copied to the SD card**. So if you drag a bunch of files onto the card all at once, the Mac may copy them in parallel and scramble the order, even though they're named perfectly. The player will then play "file 5" as whichever file happened to land 5th — not the one named `005_`.

**The fix:** use the `copy_to_sd.sh` script. It copies files one at a time in numerical order, so prefix order = copy order = playback order.

### Using the copy script

1. Plug your SD card into the Mac
2. Open **Terminal** (Cmd+Space, type "Terminal", press Enter)
3. Run:
   ```
   cd ~/Development/Annoyer
   ./copy_to_sd.sh
   ```
4. Type your Mac password when it asks
5. Wait ~30 seconds for it to finish
6. Eject the card and put it back in the device

The script also strips out the hidden `.` files macOS leaves behind on FAT32 cards. Those files confuse the DFPlayer and have to go.

### Getting the project files

Everything — the Arduino code, the scripts, this guide — lives in a public repo:

**[github.com/snickers0129/os_annoyer](https://github.com/snickers0129/os_annoyer)**

Clone it to your computer with:
```
git clone https://github.com/snickers0129/os_annoyer.git
```

If you ever break the SD card or want to start over, this is where to get fresh copies of everything.

## Quick Troubleshooting

| Problem | Try this |
| --- | --- |
| No sound at all | Check the speaker wiring and that the SD card is fully seated |
| Hear only some of the menu prompts | The SD card may have files in the wrong order — re-copy `/01/` one file at a time, in numerical order |
| Buttons don't respond | Check your solder joints on the button pads |
| Device acts weird after a battery swap | Unplug fully for 10 seconds, then plug back in |

---

## What's Inside

- **Adafruit Trinket M0** — the microcontroller (the brain)
- **DFR0299 (DFPlayer Mini)** — the MP3 player
- **Speaker** — well, you know
- **microSD card** — holds all the sound files
- **Custom PCB** — designed for this workshop, soldered by you

---

## Want to Hack On It?

Here's what your Trinket M0 can actually do — and a few ideas if you want to keep going.

### What the Trinket M0 Can Do

It's an ARM Cortex-M0+ chip running at 48 MHz. Small chip, but capable. It has:

- **256 KB of flash** — your current code uses less than 5% of that
- **32 KB of RAM**
- A **real-time clock** (already used for the Annoyer's sleep timers)
- A **deep-sleep mode** that pulls only a few microamps
- A **USB port** for re-flashing the code
- A **hardware random number generator**
- An **internal temperature sensor**

The catch: on **your** board, all 5 GPIO pins are wired to buttons, MP3 power, and the serial line. So you can't add new sensors or LEDs without modifying the PCB. But you've got a speaker and a sound player — that turns out to be a lot.

### Easy Things You Can Change Today

**Add more sounds.** Drop new MP3s into the right folder on the SD card. Name them with the next sequential 3-digit prefix (`011_…`, `012_…`, etc.). The code reads file counts at startup, so it finds them automatically — no re-flashing needed.

**Add a new mode.** In `Annoyer.ino`, find `enum Mode` near the top. Add your mode, give it an entry in `MODE_PROMPT`, add a `case` in `enterMode()`, and write the function. Same pattern as the modes you already have.

**Change the timing.** All the timing constants — how long Sound Machine plays, how often the Annoyer fires — live at the top of `Annoyer.ino`. Change a number, re-flash.

**Change the voice.** Re-run `generate_sounds.sh` after editing the text strings or voice settings. Make the menu sound like a robot, a pirate, your grandma — whatever ElevenLabs can do.

### Mode Ideas

You're limited to audio out and two buttons in. People have built a *lot* with less. Here are some starting points:

- **Joke teller.** Play loads a joke; Next loads the punchline. Cycle through your favorites.
- **Trivia game.** Plays a question, waits — Play = True, Next = False. Then it plays "Ding!" or "Wrong!"
- **Reaction timer.** Counts down with beeps, plays "GO!", times how fast a button gets pressed. Plays your time back. Two-player mode = bragging rights.
- **Magic 8-Ball.** Both buttons → plays a random fortune from a folder you fill with your own predictions.
- **Drum pad.** Play = kick, Next = snare. Tap out a beat.
- **Pomodoro timer.** 25 minutes of silence, then "Break time!"; 5 minutes of silence, then "Back to work!" Useful for actual homework.
- **Birthday card.** Holds your custom message and song for one specific person. Hand it to them on their birthday.
- **D&D sound board.** Sword clangs, dragon roars, tavern music, "Critical hit!" sound. Carry it to game night.
- **Custom alarm clock.** Wakes you at the same time every morning. You already have most of the logic — Daily Annoyer would just need to know the time of day.
- **Language flashcards.** Cycle through vocabulary. Play replays. Test yourself before the quiz.
- **Voice memos for someone you love.** Record messages at home, drop them on the card, gift the device. Grandparents will lose it.

### Going Further (Future Project)

If you ever build a second board with extra pins exposed, the Trinket M0 can also:

- Read analog sensors (light, temperature, distance, microphone)
- Drive a strip of NeoPixel LEDs
- Talk to other chips over I²C or SPI
- Detect taps and motion with an accelerometer
- Drive a tiny OLED display

That's a *whole second project*. But it starts here.

---

## A Final Word

The Hourly Annoyer is *hilarious* for about three hours. Then it stops being funny. Hide it somewhere you can't reach for the full experience.

Use responsibly. Or don't. We're not your parents.

---

*Built at the Omnisight Job Shadow Day 2026.*
*University School of the Lowcountry × Omnisight USA.*
