#include <Arduino.h>
#line 1 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
// ATtiny85 + 4x WS2812B (NeoPixel) - 8 Modes (6 normal + 2 hidden)
// Data on D3 (PB3), Button on D4 (PB4, active LOW with internal pull-up)
//
// Normal Modes (cycled with short press):
// 1) Slow fade loop through RGBCMYW
// 2) Slow fade loop through RGBCMYW + glitter
// 3) Theater chase cycling R -> G -> B
// 4) Night-light: single red LED with gentle overlap to next
// 5) KITT scanner in red with tail
// 6) Flashlight full white (max brightness)
//
// Hidden Modes (long-press to access):
// 7) Morse Code: one LED per word (R,G,B,W) for "SECRET CODEWORD IS VICTORY"
//    Long-press 3 seconds to enter.
// 8) Party Mode: full brightness; random LED, random colors (RGBCMYW), random intervals
//    Long-press 6 seconds to enter.
//
// Board: ATtiny85 @ 8 MHz (internal)
// Library: Adafruit_NeoPixel

#include <Adafruit_NeoPixel.h>

#define LED_PIN       3  // D3 (PB3)
#define BUTTON_PIN    4  // D4 (PB4) to GND, uses INPUT_PULLUP
#define NUM_LEDS      4

// ---------------------- Global brightness ----------------------
const uint8_t BRIGHTNESS_GLOBAL     = 75;   // Modes 1..5 and Mode 7 (unless changed)
const uint8_t BRIGHTNESS_FLASHLIGHT = 255;  // Mode 6 only

// ---------------------- Tunable parameters ---------------------
// Mode 1 (fade RGBCMYW)
uint16_t M1_FRAME_MS     = 35;     // tick rate
uint16_t M1_FADE_STEPS   = 180;    // bigger = slower crossfade

// Mode 2 (fade + glitter)
uint16_t M2_FRAME_MS     = 35;
uint16_t M2_FADE_STEPS   = 180;
uint16_t M2_GLITTER_PERMILLE = 80; // 0..1000 chance per tick (~8%)

// Mode 3 (theater chase RGB)
uint16_t M3_FRAME_MS     = 110;    // step speed
uint8_t  M3_CYCLE_TICKS  = 20;      // steps before changing color (R->G->B)

// Mode 4 (Night-light: single red LED with overlap to the next)
uint16_t M4_FRAME_MS        = 25;   // tick rate for Mode 4
uint16_t M4_SEG_STEPS       = 400;  // steps each LED stays active
uint16_t M4_OVERLAP_STEPS   = 80;   // how long fade overlaps
uint8_t  M4_BRIGHTNESS      = 40;   // brightness just for Night-light mode

// Mode 5 (KITT scanner red)
uint16_t M5_FRAME_MS     = 256;   // how fast to scan, lower is faster
uint8_t  M5_TAIL_L1      = 60;    // brightness of immediate neighbors
uint8_t  M5_TAIL_L2      = 1;     // two steps away

// Mode 6 (flashlight)
uint16_t M6_FRAME_MS     = 60;     // not critical

// Mode 7 (Morse Code)
uint16_t M7_FRAME_MS     = 10;     // event checker interval
uint16_t M7_UNIT_MS      = 120;    // Morse time unit (dot)
uint8_t  M7_LETTER_GAP   = 3;      // units between letters
uint8_t  M7_WORD_GAP     = 7;      // units between words

// Mode 8 (Party)
uint16_t M8_FRAME_MS     = 10;     // event checker interval
uint16_t M8_MIN_MS       = 60;     // min interval between blinks
uint16_t M8_MAX_MS       = 220;    // max interval between blinks

// Button long-press thresholds
const uint16_t LONGPRESS_MORSE_MS = 3000;  // 3 seconds -> Mode 7
const uint16_t LONGPRESS_PARTY_MS = 6000;  // 6 seconds -> Mode 8

// ---------------------------------------------------------------
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Button debounce
bool     btnState      = HIGH;
bool     btnPrev       = HIGH;
uint32_t btnLastChange = 0;
const uint16_t BTN_DEBOUNCE_MS = 35;
uint32_t pressStartMs  = 0;

// Timing
uint32_t nowMs;
uint32_t lastFrameMs = 0;
uint16_t frameIntervalMs = 30;

// Modes
// Normal modes are 0..5 (Mode1..Mode6). Hidden: 6 (Mode7), 7 (Mode8)
uint8_t  currentMode = 0;         // 0..7
const uint8_t MODE_COUNT = 6;     // short-press cycles among first 6

// ---------------------- Utilities ----------------------
#line 95 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
#line 98 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
static uint8_t qadd8(uint8_t a, uint8_t b);
#line 100 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void showAll(uint32_t color);
#line 104 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void clearAll();
#line 119 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
static uint8_t lerp8(uint8_t a, uint8_t b, uint16_t t, uint16_t tmax);
#line 128 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void drawPaletteFadeBase(uint16_t steps);
#line 135 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void advancePaletteFade(uint16_t steps);
#line 163 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void drawTheaterChase(uint8_t cycleTicks);
#line 186 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void drawNightLight(uint16_t segSteps, uint16_t overlapSteps);
#line 218 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void drawKITT(uint8_t tail1, uint8_t tail2);
#line 236 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void drawFlashlight();
#line 256 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void morseEncode(char ch, uint8_t* len, uint8_t* pat);
#line 289 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
uint32_t m7_wordColor(uint8_t w);
#line 298 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void m7_setOn(bool on);
#line 307 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void m7_begin();
#line 317 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode7();
#line 368 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
uint32_t colorRGBCMYW(uint8_t idx);
#line 380 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode8();
#line 398 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode1();
#line 403 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode2();
#line 409 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode3();
#line 413 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode4();
#line 417 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode5();
#line 421 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void Mode6();
#line 426 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void setFrameIntervalForMode();
#line 439 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void resetPerModeState();
#line 469 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void setMode(uint8_t idx);
#line 475 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void nextMode();
#line 482 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void setup();
#line 494 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
void loop();
#line 95 "C:\\Users\\TJ\\Documents\\Repos\\KidsBadge-2025\\2025-Code-KidsBadge\\2025-Code-KidsBadge.ino"
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return strip.Color(r, g, b);
}
static inline uint8_t qadd8(uint8_t a, uint8_t b) { uint16_t s = a + b; return (s > 255) ? 255 : s; }

void showAll(uint32_t color) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}
void clearAll() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0);
}

// ---------------------- Palette for Modes 1 & 2 ----------------
const uint8_t PALETTE_LEN = 7; // R,G,B,C,M,Y,W
const uint8_t paletteR[PALETTE_LEN] = {255,   0,   0,   0, 255, 255, 255};
const uint8_t paletteG[PALETTE_LEN] = {  0, 255,   0, 255,   0, 255, 255};
const uint8_t paletteB[PALETTE_LEN] = {  0,   0, 255, 255, 255,   0, 255};

uint8_t  palIdxFrom = 0;
uint8_t  palIdxTo   = 1;
uint16_t fadeStep   = 0;

// Integer lerp
static inline uint8_t lerp8(uint8_t a, uint8_t b, uint16_t t, uint16_t tmax) {
  int16_t diff = int16_t(b) - int16_t(a);
  int32_t num  = int32_t(diff) * int32_t(t);
  int16_t add  = (num >= 0) ? (num / int16_t(tmax)) : -((-num) / int16_t(tmax));
  int16_t res  = int16_t(a) + add;
  if (res < 0) res = 0; if (res > 255) res = 255;
  return uint8_t(res);
}

void drawPaletteFadeBase(uint16_t steps) {
  uint8_t r = lerp8(paletteR[palIdxFrom], paletteR[palIdxTo], fadeStep, steps);
  uint8_t g = lerp8(paletteG[palIdxFrom], paletteG[palIdxTo], fadeStep, steps);
  uint8_t b = lerp8(paletteB[palIdxFrom], paletteB[palIdxTo], fadeStep, steps);
  for (uint8_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, r, g, b);
}

void advancePaletteFade(uint16_t steps) {
  if (++fadeStep > steps) {
    fadeStep = 0;
    palIdxFrom = palIdxTo;
    palIdxTo = (palIdxTo + 1) % PALETTE_LEN;
  }
}

// Glitter (brief white speckle for this frame only)
void addGlitter(uint16_t chancePermille /*0..1000*/) {
  if ((uint16_t)random(1000) < chancePermille) {
    uint8_t i = random(NUM_LEDS);
    uint32_t c = strip.getPixelColor(i);
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8)  & 0xFF;
    uint8_t b =  c        & 0xFF;
    r = qadd8(r, 180);
    g = qadd8(g, 180);
    b = qadd8(b, 180);
    strip.setPixelColor(i, r, g, b);
  }
}

// ---------------------- Mode 3: theater chase ------------------
uint8_t chaseOffset = 0;     // 0..2
uint8_t chaseColorIdx = 0;   // 0=R,1=G,2=B
uint8_t chaseTickCount = 0;

void drawTheaterChase(uint8_t cycleTicks) {
  uint8_t r = (chaseColorIdx == 0) ? 255 : 0;
  uint8_t g = (chaseColorIdx == 1) ? 255 : 0;
  uint8_t b = (chaseColorIdx == 2) ? 255 : 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if ((i + chaseOffset) % 3 == 0) strip.setPixelColor(i, r, g, b);
    else                            strip.setPixelColor(i, 0, 0, 0);
  }
  if (++chaseOffset >= 3) chaseOffset = 0;

  if (++chaseTickCount >= cycleTicks) {
    chaseTickCount = 0;
    chaseColorIdx = (chaseColorIdx + 1) % 3; // R->G->B->R
  }
}

// ---------------------- Mode 4: Night-light --------------------
// One LED fully on in red; during the last OVERLAP portion it fades down
// while the NEXT LED fades up. At most two LEDs are lit at once.
uint16_t m4_step = 0;      // 0..M4_SEG_STEPS-1 within current LED segment
uint8_t  m4_led  = 0;      // which LED is currently active (0..NUM_LEDS-1)

void drawNightLight(uint16_t segSteps, uint16_t overlapSteps) {
  if (overlapSteps > segSteps) overlapSteps = segSteps; // clamp

  bool inOverlap = (m4_step >= (segSteps - overlapSteps));
  uint16_t overlapProg = inOverlap ? (m4_step - (segSteps - overlapSteps)) : 0; // 0..overlapSteps-1

  uint8_t currLevel, nextLevel = 0;
  if (!inOverlap) {
    currLevel = 255; // fully on during the non-overlap portion
  } else {
    uint8_t down = uint8_t((uint32_t)(overlapSteps - overlapProg) * 255UL / overlapSteps); // 255->0
    uint8_t up   = uint8_t((uint32_t)overlapProg * 255UL / overlapSteps);                  // 0->255
    currLevel = down;
    nextLevel = up;
  }

  uint8_t nextLed = (m4_led + 1) % NUM_LEDS;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (i == m4_led)            strip.setPixelColor(i, currLevel, 0, 0);
    else if (inOverlap && i == nextLed) strip.setPixelColor(i, nextLevel, 0, 0);
    else                        strip.setPixelColor(i, 0, 0, 0);
  }

  if (++m4_step >= segSteps) {
    m4_step = 0;
    m4_led  = nextLed;
  }
}

// ---------------------- Mode 5: KITT scanner -------------------
int8_t  scanPos = 0;
int8_t  scanDir = 1;
void drawKITT(uint8_t tail1, uint8_t tail2) {
  clearAll();

  for (int8_t i = 0; i < (int8_t)NUM_LEDS; i++) {
    int8_t d = i - scanPos;
    uint8_t level = 0;
    if (d == 0) level = 255;
    else if (d == 1 || d == -1) level = tail1;
    else if (d == 2 || d == -2) level = tail2;
    if (level) strip.setPixelColor(i, level, 0, 0);
  }

  scanPos += scanDir;
  if (scanPos <= 0)                  { scanPos = 0;               scanDir = +1; }
  if (scanPos >= (int8_t)NUM_LEDS-1) { scanPos = NUM_LEDS - 1;    scanDir = -1; }
}

// ---------------------- Mode 6: flashlight ---------------------
void drawFlashlight() {
  strip.setBrightness(BRIGHTNESS_FLASHLIGHT);  // ensure max while active
  showAll(rgb(255,255,255));
}

// ---------------------- Mode 7: Morse Code ---------------------
// Message split into four words; one LED per word with distinct color R,G,B,W
const uint8_t M7_WORD_COUNT = 4;
char mode7Words[M7_WORD_COUNT][10] = { "SECRET", "CODEWORD", "IS", "VICTORY" };

// State
uint8_t  m7_word = 0;     // 0..3
uint8_t  m7_char = 0;     // index within current word
uint8_t  m7_sym  = 0;     // element within character
uint8_t  m7_charLen = 0;  // number of elements in current char
uint8_t  m7_charPat = 0;  // bits: MSB-first, 0=dot,1=dash
bool     m7_onPhase = false;
uint32_t m7_nextChangeMs = 0;

// Encode A..Z to (len, pattern). Pattern MSB-first, 0=dot,1=dash
void morseEncode(char ch, uint8_t* len, uint8_t* pat) {
  if (ch >= 'a' && ch <= 'z') ch -= 32; // to upper
  switch (ch) {
    case 'A': *len=2; *pat=0b01;   break; // .-
    case 'B': *len=4; *pat=0b1000; break; // -...
    case 'C': *len=4; *pat=0b1010; break; // -.-.
    case 'D': *len=3; *pat=0b100;  break; // -..
    case 'E': *len=1; *pat=0b0;    break; // .
    case 'F': *len=4; *pat=0b0010; break; // ..-.
    case 'G': *len=3; *pat=0b110;  break; // --.
    case 'H': *len=4; *pat=0b0000; break; // ....
    case 'I': *len=2; *pat=0b00;   break; // ..
    case 'J': *len=4; *pat=0b0111; break; // .---
    case 'K': *len=3; *pat=0b101;  break; // -.-
    case 'L': *len=4; *pat=0b0100; break; // .-..
    case 'M': *len=2; *pat=0b11;   break; // --
    case 'N': *len=2; *pat=0b10;   break; // -.
    case 'O': *len=3; *pat=0b111;  break; // ---
    case 'P': *len=4; *pat=0b0110; break; // .--.
    case 'Q': *len=4; *pat=0b1101; break; // --.-
    case 'R': *len=3; *pat=0b010;  break; // .-.
    case 'S': *len=3; *pat=0b000;  break; // ...
    case 'T': *len=1; *pat=0b1;    break; // -
    case 'U': *len=3; *pat=0b001;  break; // ..-
    case 'V': *len=4; *pat=0b0001; break; // ...-
    case 'W': *len=3; *pat=0b011;  break; // .--
    case 'X': *len=4; *pat=0b1001; break; // -..-
    case 'Y': *len=4; *pat=0b1011; break; // -.--
    case 'Z': *len=4; *pat=0b1100; break; // --..
    default:  *len=0; *pat=0;      break; // unsupported
  }
}

uint32_t m7_wordColor(uint8_t w) {
  switch (w % 4) {
    case 0: return rgb(255,0,0);   // R
    case 1: return rgb(0,255,0);   // G
    case 2: return rgb(0,0,255);   // B
    default:return rgb(255,255,255); // W
  }
}

void m7_setOn(bool on) {
  clearAll();
  if (on) {
    uint8_t led = m7_word % NUM_LEDS;
    strip.setPixelColor(led, m7_wordColor(m7_word));
  }
  strip.show();
}

void m7_begin() {
  m7_word = 0; m7_char = 0; m7_sym = 0; m7_onPhase = true;
  char ch = mode7Words[m7_word][m7_char];
  morseEncode(ch, &m7_charLen, &m7_charPat);
  if (m7_charLen == 0) { m7_onPhase = false; m7_nextChangeMs = millis() + (uint32_t)M7_UNIT_MS; return; }
  uint8_t units = ((m7_charPat >> (m7_charLen - 1 - m7_sym)) & 0x01) ? 3 : 1;
  m7_setOn(true);
  m7_nextChangeMs = millis() + (uint32_t)units * M7_UNIT_MS;
}

void Mode7() {
  uint32_t now = millis();
  if ((int32_t)(now - m7_nextChangeMs) < 0) return; // not yet time

  if (m7_onPhase) {
    // Finished ON phase of current symbol -> go OFF
    m7_setOn(false);
    m7_onPhase = false;
    if (m7_sym < m7_charLen - 1) {
      // intra-character gap (1 unit)
      m7_nextChangeMs = now + M7_UNIT_MS;
    } else {
      // End of letter: letter gap or word gap
      bool endOfWord = (mode7Words[m7_word][m7_char+1] == '\0');
      uint16_t gapUnits = endOfWord ? M7_WORD_GAP : M7_LETTER_GAP;
      m7_nextChangeMs = now + (uint32_t)gapUnits * M7_UNIT_MS;
    }
  } else {
    // Finished OFF phase -> either next symbol, next letter, or next word
    if (m7_sym < m7_charLen - 1) {
      // Next symbol in same letter
      m7_sym++;
      m7_onPhase = true;
      uint8_t units = ((m7_charPat >> (m7_charLen - 1 - m7_sym)) & 0x01) ? 3 : 1;
      m7_setOn(true);
      m7_nextChangeMs = now + (uint32_t)units * M7_UNIT_MS;
    } else {
      // Move to next letter or next word
      if (mode7Words[m7_word][m7_char+1] != '\0') {
        // next letter
        m7_char++;
      } else {
        // next word
        m7_word = (m7_word + 1) % M7_WORD_COUNT;
        m7_char = 0;
      }
      // Load new character
      char ch = mode7Words[m7_word][m7_char];
      morseEncode(ch, &m7_charLen, &m7_charPat);
      m7_sym = 0;
      m7_onPhase = true;
      uint8_t units = ((m7_charPat >> (m7_charLen - 1 - m7_sym)) & 0x01) ? 3 : 1;
      m7_setOn(true);
      m7_nextChangeMs = now + (uint32_t)units * M7_UNIT_MS;
    }
  }
}

// ---------------------- Mode 8: Party Mode ---------------------
uint32_t m8_nextChangeMs = 0;

uint32_t colorRGBCMYW(uint8_t idx) {
  switch (idx % 7) {
    case 0: return rgb(255,0,0);     // R
    case 1: return rgb(0,255,0);     // G
    case 2: return rgb(0,0,255);     // B
    case 3: return rgb(0,255,255);   // C
    case 4: return rgb(255,0,255);   // M
    case 5: return rgb(255,255,0);   // Y
    default:return rgb(255,255,255); // W
  }
}

void Mode8() {
  uint32_t now = millis();
  if ((int32_t)(now - m8_nextChangeMs) < 0) return; // wait until next blink

  // Choose random LED and color
  uint8_t led = random(NUM_LEDS);
  uint32_t col = colorRGBCMYW(random(7));

  clearAll();
  strip.setPixelColor(led, col);
  strip.show();

  // Schedule next change
  uint16_t delta = M8_MIN_MS + (random(M8_MAX_MS - M8_MIN_MS + 1));
  m8_nextChangeMs = now + delta;
}

// ---------------------- Mode wrappers --------------------------
void Mode1() {
  drawPaletteFadeBase(M1_FADE_STEPS);
  advancePaletteFade(M1_FADE_STEPS);
  strip.show();
}
void Mode2() {
  drawPaletteFadeBase(M2_FADE_STEPS);
  addGlitter(M2_GLITTER_PERMILLE);
  strip.show();
  advancePaletteFade(M2_FADE_STEPS);
}
void Mode3() {
  drawTheaterChase(M3_CYCLE_TICKS);
  strip.show();
}
void Mode4() {
  drawNightLight(M4_SEG_STEPS, M4_OVERLAP_STEPS);
  strip.show();
}
void Mode5() {
  drawKITT(M5_TAIL_L1, M5_TAIL_L2);
  strip.show();
}
void Mode6() {
  drawFlashlight();
}

// ---------------------- Mode mgmt / setup ----------------------
void setFrameIntervalForMode() {
  switch (currentMode) {
    case 0: frameIntervalMs = M1_FRAME_MS; break;
    case 1: frameIntervalMs = M2_FRAME_MS; break;
    case 2: frameIntervalMs = M3_FRAME_MS; break;
    case 3: frameIntervalMs = M4_FRAME_MS; break;
    case 4: frameIntervalMs = M5_FRAME_MS; break;
    case 5: frameIntervalMs = M6_FRAME_MS; break;
    case 6: frameIntervalMs = M7_FRAME_MS; break; // Morse
    case 7: frameIntervalMs = M8_FRAME_MS; break; // Party
  }
}

void resetPerModeState() {
  // Reset shared state
  palIdxFrom = 0; palIdxTo = 1; fadeStep = 0;
  chaseOffset = 0; chaseColorIdx = 0; chaseTickCount = 0;
  m4_step = 0; m4_led = 0;
  scanPos = 0; scanDir = +1;
  m7_word = 0; m7_char = 0; m7_sym = 0; m7_onPhase = false; m7_nextChangeMs = millis();
  m8_nextChangeMs = millis();

  // Brightness defaults (global), with per-mode overrides
  strip.setBrightness(BRIGHTNESS_GLOBAL);
  if (currentMode == 3) { // Mode4 Night-light custom brightness
    strip.setBrightness(M4_BRIGHTNESS);
  }
  if (currentMode == 5) { // Mode6 Flashlight
    strip.setBrightness(BRIGHTNESS_FLASHLIGHT);
  }
  if (currentMode == 7) { // Mode8 Party Mode: full brightness
    strip.setBrightness(255);
  }

  // Special inits
  if (currentMode == 6) { // Mode7 Morse
    m7_begin();
  }
  if (currentMode == 7) { // Mode8 Party: force immediate first blink
    m8_nextChangeMs = millis();
  }
}

void setMode(uint8_t idx) {
  currentMode = idx;
  resetPerModeState();
  setFrameIntervalForMode();
}

void nextMode() {
  // Only cycle within normal modes 0..5
  currentMode = (currentMode + 1) % MODE_COUNT;
  resetPerModeState();
  setFrameIntervalForMode();
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  strip.begin();
  strip.setBrightness(BRIGHTNESS_GLOBAL);
  strip.show(); // clear

  randomSeed(analogRead(0));
  currentMode = 0;             // start at Mode1
  resetPerModeState();
  setFrameIntervalForMode();
}

void loop() {
  nowMs = millis();

  // Debounced button
  bool raw = digitalRead(BUTTON_PIN);
  if (raw != btnPrev) { btnLastChange = nowMs; btnPrev = raw; }
  if ((nowMs - btnLastChange) > BTN_DEBOUNCE_MS) {
    if (raw != btnState) {
      btnState = raw;
      if (btnState == LOW) {
        // pressed
        pressStartMs = nowMs;
      } else {
        // released -> evaluate press duration
        uint32_t held = nowMs - pressStartMs;
        if (held >= LONGPRESS_PARTY_MS) {
          setMode(7); // Hidden Mode 8: Party
        } else if (held >= LONGPRESS_MORSE_MS) {
          setMode(6); // Hidden Mode 7: Morse
        } else {
          nextMode(); // short press cycles normal modes
        }
      }
    }
  }

  // Frame tick
  if ((nowMs - lastFrameMs) >= frameIntervalMs) {
    lastFrameMs = nowMs;
    switch (currentMode) {
      case 0: Mode1(); break;
      case 1: Mode2(); break;
      case 2: Mode3(); break;
      case 3: Mode4(); break;  // Night-light
      case 4: Mode5(); break;
      case 5: Mode6(); break;
      case 6: Mode7(); break;  // Morse (hidden)
      case 7: Mode8(); break;  // Party (hidden)
      default: showAll(0); break;
    }
  }
}

