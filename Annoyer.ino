/*
 * ANNOYER — multi-mode sound device for Adafruit Trinket M0 + DFR0299
 *
 * Hardware:
 *   D0  Play/OK button     (active HIGH, external pulldown, NMI - cannot wake from sleep)
 *   D1  Next button        (active HIGH, external pulldown, EXTINT[2] - wakes from sleep)
 *   D2  MP3 power control  (HIGH = transistor ON = DFR0299 powered)
 *   D3  Serial1 RX  <- DFR0299 TX
 *   D4  Serial1 TX  -> DFR0299 RX
 *
 * SD card layout (folders MUST be named with 2-digit numbers, files MUST start
 * with a 3-digit prefix - text after the prefix is for humans only):
 *
 *   /01/  System narration (recorded in this exact order on the card):
 *     001_select_mode.mp3
 *     002_sound_machine.mp3
 *     003_noise_maker.mp3
 *     004_daily_annoyer.mp3
 *     005_hourly_annoyer.mp3
 *     (slot 006 unused)
 *     007_select_sound.mp3
 *     008_starting_now.mp3
 *     009_adjust_volume.mp3
 *     010_new_volume_saved.mp3
 *     011_random_sound.mp3
 *     012_quotes.mp3
 *     013_up.mp3
 *     014_down.mp3
 *
 *   /02/  Sound Machine loops:            001_rain.mp3, 002_white_noise.mp3, ...
 *   /03/  Noise Maker / Annoyer effects:  001_fart.mp3, 002_airhorn.mp3, ...
 *   /07/  Quotes:                         001_*.mp3, 002_*.mp3, ...
 *
 * IMPORTANT: the DFPlayer reads files in the order they were copied to the card,
 * not alphabetical. Copy them one-by-one in the order above, or use AMP3FileSort
 * (or similar) to fix card order after the fact.
 *
 * Libraries (install via Library Manager):
 *   DFRobotDFPlayerMini
 *   ArduinoLowPower
 *   RTCZero
 *   FlashStorage
 */

#include <DFRobotDFPlayerMini.h>
#include <ArduinoLowPower.h>
#include <RTCZero.h>
#include <FlashStorage.h>
#include <Adafruit_DotStar.h>


// ===== Pins =====
const uint8_t PIN_BTN_PLAY  = 0;
const uint8_t PIN_BTN_NEXT  = 1;
const uint8_t PIN_MP3_POWER = 2;


// ===== Folders (must match folder names on the SD card) =====
const uint8_t FOLDER_SYSTEM        = 1;
const uint8_t FOLDER_SOUND_MACHINE = 2;
const uint8_t FOLDER_NOISE_MAKER   = 3;  // Annoyer also pulls from this folder
const uint8_t FOLDER_QUOTES        = 7;


// ===== System prompt track numbers in /01/ =====
const uint8_t SYS_SELECT_MODE        = 1;
const uint8_t SYS_MODE_SOUND_MACHINE = 2;
const uint8_t SYS_MODE_NOISE_MAKER   = 3;
const uint8_t SYS_MODE_DAILY         = 4;
const uint8_t SYS_MODE_HOURLY        = 5;
const uint8_t SYS_SELECT_SOUND       = 7;
const uint8_t SYS_STARTING_NOW       = 8;
const uint8_t SYS_ADJUST_VOLUME      = 9;
const uint8_t SYS_VOLUME_SAVED       = 10;
const uint8_t SYS_RANDOM_SOUND       = 11;
const uint8_t SYS_MODE_QUOTES        = 12;
const uint8_t SYS_UP                 = 13;
const uint8_t SYS_DOWN               = 14;

// Sentinel returned by selectSound() when the user picks "Random Sound".
// 255 is safe: numbered folders on the DFPlayer hold at most 255 files.
const uint8_t SOUND_RANDOM = 255;


// ===== Modes =====
enum Mode {
  MODE_SOUND_MACHINE  = 0,
  MODE_NOISE_MAKER    = 1,
  MODE_QUOTES         = 2,
  MODE_DAILY_ANNOYER  = 3,
  MODE_HOURLY_ANNOYER = 4,
  NUM_MODES
};

const uint8_t MODE_PROMPT[NUM_MODES] = {
  SYS_MODE_SOUND_MACHINE,
  SYS_MODE_NOISE_MAKER,
  SYS_MODE_QUOTES,
  SYS_MODE_DAILY,
  SYS_MODE_HOURLY
};


// ===== Timing =====
const uint32_t SOUND_MACHINE_MS = 60UL * 60UL * 1000UL;       // 60 min
const uint32_t HOURLY_MS        = 60UL * 60UL * 1000UL;       // 1 hour
const uint32_t DAILY_MS         = 24UL * 60UL * 60UL * 1000UL; // 24 hours
const uint32_t SLEEP_CHUNK_MS   = 60UL * 60UL * 1000UL;       // 1 hour per LowPower call
const uint32_t VOL_TIMEOUT_MS   = 10000;                       // 10s of inactivity = save & exit
const uint32_t CONFIRM_PLAY_MS  = 5000;                        // preview clip cap before "Starting Now"
const uint32_t DEBOUNCE_MS      = 30;
const uint32_t BOTH_WINDOW_MS   = 250;                         // how long to wait to detect both-press


// ===== Volume =====
const uint8_t MIN_VOL     = 5;
const uint8_t MAX_VOL     = 30;
const uint8_t DEFAULT_VOL = 22;


// ===== Globals =====

// NOTE: this enum must be declared BEFORE the first function definition in
// the file. Arduino IDE auto-generates function prototypes and inserts them
// right above the first function it sees — if Button is declared after
// those helpers, the prototypes for pollButtons()/waitForButton() reference
// an undeclared type and the sketch won't compile.
enum Button { BTN_NONE, BTN_PLAY, BTN_NEXT, BTN_BOTH };

DFRobotDFPlayerMini mp3;
FlashStorage(volStore, uint8_t);
uint8_t volNow = DEFAULT_VOL;
volatile bool wakeFlag = false;

// Onboard DotStar RGB LED. Used as a power-on indicator and for button-press
// feedback. Disabled entirely once we enter an Annoyer mode (battery first).
Adafruit_DotStar dotstar(1, PIN_DOTSTAR_DATA, PIN_DOTSTAR_CLK, DOTSTAR_BRG);
bool ledEnabled = true;

void setDotStar(uint8_t r, uint8_t g, uint8_t b) {
  dotstar.setPixelColor(0, r, g, b);
  dotstar.show();
}

void clearDotStar() {
  dotstar.clear();
  dotstar.show();
}

// Brief blink to confirm a button press. No-op when ledEnabled is false.
void flashDotStar() {
  if (!ledEnabled) return;
  setDotStar(0, 80, 0);   // green
  delay(40);
  clearDotStar();
}

// Cached folder file counts, indexed by folder number. Populated once at boot
// while the system is quiet — runtime queries get scrambled by interleaved
// play-finish events from the DFPlayer.
int folderFileCount[8] = {0};


// ===========================================================================
// Arduino entry points
// ===========================================================================

void setup() {
  // Light the DotStar green for 5 seconds at boot so the user knows the
  // device is on. Setup work (MP3 player init, folder count caching) takes
  // a few seconds already; we pad to a guaranteed 5 s total.
  dotstar.begin();
  setDotStar(0, 80, 0);
  unsigned long bootStart = millis();

  pinMode(PIN_BTN_PLAY,  INPUT);
  pinMode(PIN_BTN_NEXT,  INPUT);
  pinMode(PIN_MP3_POWER, OUTPUT);
  digitalWrite(PIN_MP3_POWER, LOW);

  loadVolume();
  mp3PowerOn();

  // Ensure boot LED is visible for at least 5 seconds total
  while (millis() - bootStart < 5000) {
    delay(10);
  }
  clearDotStar();

  runMenu();
}

void loop() {
  // unused - everything runs from setup() -> runMenu() and never returns
}


// ===========================================================================
// Persistent volume (survives power cycles)
// ===========================================================================

void loadVolume() {
  uint8_t v = volStore.read();
  volNow = (v >= MIN_VOL && v <= MAX_VOL) ? v : DEFAULT_VOL;
}

void saveVolume() {
  volStore.write(volNow);
}


// ===========================================================================
// MP3 player power + serial bring-up
// ===========================================================================

void mp3PowerOn() {
  Serial.begin(115200);         // USB debug — open Serial Monitor at 115200
  digitalWrite(PIN_MP3_POWER, HIGH);
  delay(1500);                  // DFR0299 needs ~1s after power-on to be ready
  Serial1.begin(9600);
  delay(200);

  // First try with ack (proves both directions work).
  Serial.println("[mp3] Attempting init with ack...");
  bool ok = false;
  for (int i = 0; i < 3; i++) {
    if (mp3.begin(Serial1, /*isACK=*/true, /*doReset=*/false)) {
      ok = true;
      Serial.print("[mp3] ACK init OK on attempt ");
      Serial.println(i + 1);
      break;
    }
    Serial.print("[mp3] ACK attempt ");
    Serial.print(i + 1);
    Serial.println(" failed");
    delay(500);
  }

  // Fall back to no-ack mode (commands sent blindly; works even if the
  // DFPlayer-to-Trinket line is broken).
  if (!ok) {
    Serial.println("[mp3] Falling back to isACK=false");
    mp3.begin(Serial1, /*isACK=*/false, /*doReset=*/true);
    delay(500);
  }

  mp3.volume(volNow);
  delay(50);
  Serial.print("[mp3] Volume set to ");
  Serial.println(volNow);

  // Cache folder counts. The DFPlayer's responses can lag by one query, so
  // we drain the receive buffer before every call and discard a warm-up query.
  delay(1000);

  // Drain anything queued up
  while (mp3.available()) {
    mp3.readType();
    mp3.read();
  }

  // Warm-up query (its response will arrive late and gets discarded)
  mp3.readFileCountsInFolder(1);
  delay(500);
  while (mp3.available()) {
    mp3.readType();
    mp3.read();
  }

  Serial.println("[mp3] Caching folder counts:");
  const uint8_t folders[] = {1, 2, 3, 7};
  for (uint8_t i = 0; i < sizeof(folders); i++) {
    uint8_t f = folders[i];

    // Drain stale events right before each query
    while (mp3.available()) {
      mp3.readType();
      mp3.read();
    }

    int c = mp3.readFileCountsInFolder(f);
    folderFileCount[f] = (c > 0 && c <= 255) ? c : 0;
    Serial.print("  /0");
    Serial.print(f);
    Serial.print("/: raw=");
    Serial.print(c);
    Serial.print(" cached=");
    Serial.println(folderFileCount[f]);
    delay(500);  // longer delay between queries
  }
}

void mp3PowerOff() {
  Serial1.end();
  digitalWrite(PIN_MP3_POWER, LOW);
}


// ===========================================================================
// Playback helpers
// ===========================================================================

// True if a "track finished" message is waiting on the serial bus.
bool trackFinished() {
  if (!mp3.available()) return false;
  return mp3.readType() == DFPlayerPlayFinished;
}

// Block until the current track ends (or timeout, as a safety net).
void waitForTrackEnd(uint32_t timeoutMs) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (mp3.available()) {
      uint8_t type = mp3.readType();
      int value = mp3.read();
      Serial.print("[mp3] event type=");
      Serial.print(type);
      Serial.print(" value=");
      Serial.println(value);
      if (type == DFPlayerPlayFinished) { delay(50); return; }
    }
    delay(20);
  }
  Serial.println("[mp3] waitForTrackEnd TIMEOUT");
}

// Play a /01/ narration clip and block until it finishes.
void playSystemTrack(uint8_t track) {
  Serial.print("[mp3] playSystemTrack(");
  Serial.print(track);
  Serial.println(")");
  mp3.playFolder(FOLDER_SYSTEM, track);
  waitForTrackEnd(20000UL);
}

// Play a clip but cut it off after CONFIRM_PLAY_MS. Used when previewing
// chosen sounds (e.g. Sound Machine loops are way too long to play in full).
void playConfirmation(uint8_t folder, uint8_t track) {
  mp3.playFolder(folder, track);
  unsigned long t0 = millis();
  while (millis() - t0 < CONFIRM_PLAY_MS) {
    if (trackFinished()) break;
    delay(20);
  }
  mp3.stop();
  delay(100);
}


// ===========================================================================
// Button input - debounced, with both-press detection
// ===========================================================================

Button pollButtons() {
  bool p = digitalRead(PIN_BTN_PLAY) == HIGH;
  bool n = digitalRead(PIN_BTN_NEXT) == HIGH;
  if (!p && !n) return BTN_NONE;

  delay(DEBOUNCE_MS);
  p = digitalRead(PIN_BTN_PLAY) == HIGH;
  n = digitalRead(PIN_BTN_NEXT) == HIGH;
  if (!p && !n) return BTN_NONE;

  // Visual feedback: flash the DotStar immediately on confirmed press
  // (skipped in Annoyer modes where ledEnabled is false).
  flashDotStar();

  // Both-press detection: only commits to BTN_BOTH if BOTH pins are HIGH
  // continuously for BOTH_HOLD_MS. Single momentary flickers on the other
  // pin (electrical coupling between traces, finger brushes) do NOT count.
  const uint32_t BOTH_HOLD_MS = 50;
  unsigned long bothHighSince = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < BOTH_WINDOW_MS) {
    bool pp = digitalRead(PIN_BTN_PLAY) == HIGH;
    bool nn = digitalRead(PIN_BTN_NEXT) == HIGH;
    if (pp && nn) {
      if (bothHighSince == 0) bothHighSince = millis();
      if (millis() - bothHighSince >= BOTH_HOLD_MS) {
        p = n = true;   // confirmed both-press
        break;
      }
    } else {
      bothHighSince = 0;  // reset the streak
    }
    delay(5);
  }

  // Wait for full release before returning - prevents repeat-trigger.
  while (digitalRead(PIN_BTN_PLAY) == HIGH || digitalRead(PIN_BTN_NEXT) == HIGH) {
    delay(5);
  }
  delay(DEBOUNCE_MS);

  if (p && n) return BTN_BOTH;
  if (p)      return BTN_PLAY;
  return BTN_NEXT;
}

Button waitForButton() {
  while (true) {
    Button b = pollButtons();
    if (b != BTN_NONE) return b;
    delay(10);
  }
}


// ===========================================================================
// Volume Adjust (overlay - reachable from anywhere via both-button press)
// Next = up, Play = down, 10s of inactivity saves and exits.
// ===========================================================================

void adjustVolume() {
  mp3.stop();
  delay(100);
  playSystemTrack(SYS_ADJUST_VOLUME);

  unsigned long lastInputAt = millis();
  while (millis() - lastInputAt < VOL_TIMEOUT_MS) {
    Button b = pollButtons();
    if (b == BTN_NEXT && volNow < MAX_VOL) {
      volNow++;
      mp3.volume(volNow);
      delay(40);
      mp3.playFolder(FOLDER_SYSTEM, SYS_UP);  // hear "Up" at the new level
      lastInputAt = millis();
    } else if (b == BTN_PLAY && volNow > MIN_VOL) {
      volNow--;
      mp3.volume(volNow);
      delay(40);
      mp3.playFolder(FOLDER_SYSTEM, SYS_DOWN);  // hear "Down" at the new level
      lastInputAt = millis();
    } else if (b == BTN_BOTH) {
      break;  // user asked to exit early
    }
    delay(10);
  }

  mp3.stop();
  delay(100);
  saveVolume();
  playSystemTrack(SYS_VOLUME_SAVED);
}


// ===========================================================================
// Main menu - announces modes, dispatches on Play
// ===========================================================================

void runMenu() {
  uint8_t mode = MODE_SOUND_MACHINE;

  playSystemTrack(SYS_SELECT_MODE);
  playSystemTrack(MODE_PROMPT[mode]);

  while (true) {
    Button b = waitForButton();
    if (b == BTN_NEXT) {
      mode = (mode + 1) % NUM_MODES;
      playSystemTrack(MODE_PROMPT[mode]);
    } else if (b == BTN_PLAY) {
      enterMode(mode);
      // If enterMode returns (e.g. mode had no sounds), restart from the top.
      mode = MODE_SOUND_MACHINE;
      playSystemTrack(SYS_SELECT_MODE);
      playSystemTrack(MODE_PROMPT[mode]);
    } else if (b == BTN_BOTH) {
      adjustVolume();
      playSystemTrack(MODE_PROMPT[mode]);
    }
  }
}


// ===========================================================================
// Sound selection - cycle through tracks in a folder, hear each, then OK.
// When allowRandom is true, "Random Sound" is offered as the first option;
// selecting it returns SOUND_RANDOM so the caller can pick a different track
// each time the mode fires.
//
// Returns:  1..total  = a specific track
//           SOUND_RANDOM = pick at random each play (only if allowRandom)
//           0         = folder has no sounds and random was not allowed
// ===========================================================================

void playSelectPreview(uint8_t folder, uint8_t current) {
  if (current == SOUND_RANDOM) {
    mp3.playFolder(FOLDER_SYSTEM, SYS_RANDOM_SOUND);
  } else {
    mp3.playFolder(folder, current);
  }
}

uint8_t selectSound(uint8_t folder, bool allowRandom) {
  int total = folderFileCount[folder];
  Serial.print("[selectSound] folder=");
  Serial.print(folder);
  Serial.print(" cached_total=");
  Serial.println(total);
  if (total <= 0) {
    // Cache says no tracks, but the cache can be unreliable. Use a fallback
    // so the user still hears something and isn't bounced back to the menu.
    total = 99;
    Serial.println("[selectSound] cache miss, using fallback total=99");
  }

  playSystemTrack(SYS_SELECT_SOUND);

  uint8_t current = allowRandom ? SOUND_RANDOM : 1;
  playSelectPreview(folder, current);

  while (true) {
    Button b = waitForButton();
    if (b == BTN_NEXT) {
      // Cycle order: [Random] -> 1 -> 2 -> ... -> total -> [Random] -> ...
      if (current == SOUND_RANDOM) {
        current = 1;
      } else if (current >= total) {
        current = allowRandom ? SOUND_RANDOM : 1;
      } else {
        current++;
      }
      mp3.stop();
      delay(80);
      playSelectPreview(folder, current);
    } else if (b == BTN_PLAY) {
      mp3.stop();
      delay(80);
      return current;
    } else if (b == BTN_BOTH) {
      mp3.stop();
      delay(80);
      adjustVolume();
      playSelectPreview(folder, current);
    }
  }
}


// ===========================================================================
// Mode dispatcher
// ===========================================================================

void enterMode(uint8_t mode) {
  Serial.print("[enterMode] mode=");
  Serial.println(mode);
  switch (mode) {
    case MODE_SOUND_MACHINE: {
      uint8_t sound = selectSound(FOLDER_SOUND_MACHINE, false);
      if (sound == 0) return;
      playSystemTrack(MODE_PROMPT[mode]);
      playConfirmation(FOLDER_SOUND_MACHINE, sound);
      playSystemTrack(SYS_STARTING_NOW);
      runSoundMachine(sound);
      break;
    }
    case MODE_NOISE_MAKER:
      runSoundBrowser(FOLDER_NOISE_MAKER);
      break;
    case MODE_QUOTES:
      runSoundBrowser(FOLDER_QUOTES);
      break;
    case MODE_DAILY_ANNOYER: {
      uint8_t sound = selectSound(FOLDER_NOISE_MAKER, true);
      if (sound == 0) return;
      playSystemTrack(MODE_PROMPT[mode]);
      if (sound != SOUND_RANDOM) playConfirmation(FOLDER_NOISE_MAKER, sound);
      playSystemTrack(SYS_STARTING_NOW);
      runAnnoyer(sound, DAILY_MS);
      break;
    }
    case MODE_HOURLY_ANNOYER: {
      uint8_t sound = selectSound(FOLDER_NOISE_MAKER, true);
      if (sound == 0) return;
      playSystemTrack(MODE_PROMPT[mode]);
      if (sound != SOUND_RANDOM) playConfirmation(FOLDER_NOISE_MAKER, sound);
      playSystemTrack(SYS_STARTING_NOW);
      runAnnoyer(sound, HOURLY_MS);
      break;
    }
  }
}


// ===========================================================================
// Sound Machine - loop chosen sound for 60 min, then deep sleep
// ===========================================================================

void runSoundMachine(uint8_t sound) {
  unsigned long t0 = millis();
  mp3.playFolder(FOLDER_SOUND_MACHINE, sound);
  delay(150);
  mp3.enableLoop();   // DFPlayer hardware loop - much tighter than restart-on-finish

  while (millis() - t0 < SOUND_MACHINE_MS) {
    Button b = pollButtons();
    if (b == BTN_BOTH) {
      mp3.disableLoop();
      adjustVolume();
      mp3.playFolder(FOLDER_SOUND_MACHINE, sound);
      delay(150);
      mp3.enableLoop();
    }
    delay(30);
  }

  mp3.disableLoop();
  mp3.stop();
  delay(200);
  mp3PowerOff();
  sleepForever();
}


// ===========================================================================
// Sound Browser - shared by Noise Maker, Memes, and Quotes.
// Next cycles to the next track and plays, Play replays current.
// ===========================================================================

void runSoundBrowser(uint8_t folder) {
  int total = folderFileCount[folder];
  Serial.print("[runSoundBrowser] folder=");
  Serial.print(folder);
  Serial.print(" cached_total=");
  Serial.println(total);
  if (total <= 0) {
    total = 99;
    Serial.println("[runSoundBrowser] cache miss, using fallback total=99");
  }
  uint8_t current = 1;

  while (true) {
    Button b = pollButtons();
    if (b == BTN_NEXT) {
      current = (current % total) + 1;
      mp3.stop();
      delay(50);
      mp3.playFolder(folder, current);
    } else if (b == BTN_PLAY) {
      mp3.stop();
      delay(50);
      mp3.playFolder(folder, current);
    } else if (b == BTN_BOTH) {
      mp3.stop();
      delay(50);
      adjustVolume();
    }
    if (mp3.available()) mp3.readType();  // drain finished events so the buffer doesn't fill
    delay(15);
  }
}


// ===========================================================================
// Annoyer - deep sleep, wake on RTC alarm OR Next button, play, sleep again
// ===========================================================================

void wakeISR() { wakeFlag = true; }

void runAnnoyer(uint8_t sound, uint32_t intervalMs) {
  LowPower.attachInterruptWakeup(PIN_BTN_NEXT, wakeISR, RISING);
  randomSeed(millis());   // seeded from how long they took to navigate the menus

  // Annoyer mode: disable the DotStar entirely. The device is asleep most
  // of the time, and any LED activity (even brief flashes on button wake)
  // chews into battery life. User explicitly wants this off here.
  ledEnabled = false;
  clearDotStar();

  while (true) {
    mp3PowerOff();
    sleepForInterval(intervalMs);

    mp3PowerOn();

    // For SOUND_RANDOM, pick a different track each cycle.
    uint8_t toPlay = sound;
    if (sound == SOUND_RANDOM) {
      int total = folderFileCount[FOLDER_NOISE_MAKER];
      toPlay = (total > 0) ? (uint8_t)(random(total) + 1) : 1;
    }
    mp3.playFolder(FOLDER_NOISE_MAKER, toPlay);

    // Wait for the sound to finish, but allow both-press for volume.
    // 5-min safety cap in case the DFPlayer never reports finished.
    unsigned long t0 = millis();
    while (millis() - t0 < 5UL * 60UL * 1000UL) {
      if (trackFinished()) break;
      Button b = pollButtons();
      if (b == BTN_BOTH) adjustVolume();
      delay(20);
    }
  }
}

// Sleep up to intervalMs in 1-hour chunks. Returns early if Next was pressed.
// Chunking keeps each LowPower call well within the RTC's comfortable range
// and makes the long 24h sleep more resilient.
void sleepForInterval(uint32_t intervalMs) {
  uint32_t remaining = intervalMs;
  wakeFlag = false;

  // Disable USB pullup before standby — saves hundreds of uA.
  // Re-enabled on wake so Serial Monitor still works during the awake window.
  USBDevice.detach();

  while (remaining > 0) {
    uint32_t chunk = remaining > SLEEP_CHUNK_MS ? SLEEP_CHUNK_MS : remaining;
    LowPower.deepSleep(chunk);
    if (wakeFlag) break;
    remaining -= chunk;
  }

  USBDevice.attach();
  wakeFlag = false;
}


// ===========================================================================
// Terminal deep sleep - used after the Sound Machine timer expires.
// Power-cycle to start over. Next button silently wakes and re-sleeps.
// ===========================================================================

void sleepForever() {
  // Terminal sleep — drop USB permanently to minimize power until power-cycle.
  USBDevice.detach();
  LowPower.attachInterruptWakeup(PIN_BTN_NEXT, wakeISR, RISING);
  while (true) {
    LowPower.deepSleep(SLEEP_CHUNK_MS);
    wakeFlag = false;
  }
}
