/*
 * ============================================================
 *  Quiz Buzzer System — ESP32 Firmware  (FINAL V4)
 *
 *  KEY FIX: Separated "disabled" (operator OFF) from "locked"
 *  (round-locked). Disabled buzzers are NEVER overwritten by
 *  any game event — their LEDs stay OFF at all times.
 * ============================================================
 *
 *  Wi-Fi AP:  BuzzerSystem  /  buzzer123
 *  IP:        192.168.4.1   Port 81 (WebSocket)
 *
 *  Pin Map:
 *    Player | LED (Yellow) | Button (Green)
 *    -------|--------------|---------------
 *       1   |   GPIO 5     |   GPIO 13
 *       2   |   GPIO 18    |   GPIO 12
 *       3   |   GPIO 19    |   GPIO 14
 *       4   |   GPIO 21    |   GPIO 27
 *       5   |   GPIO 22    |   GPIO 26
 *       6   |   GPIO 23    |   GPIO 25
 *
 *  External Buzzer: GPIO 32
 * ============================================================
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// ── Access Point ──────────────────────────────────────────────
const char* AP_NAME     = "BuzzerSystem";
const char* AP_PASSWORD = "buzzer123";

// ── Pins ──────────────────────────────────────────────────────
const uint8_t LED_PINS[6]    = {  5, 18, 19, 21, 22, 23 };
const uint8_t BUTTON_PINS[6] = { 13, 12, 14, 27, 26, 25 };
#define BUZZER_PIN 32
#define NUM_LEDS   10

// ── Colors ────────────────────────────────────────────────────
#define COL_OFF    0,   0,   0
#define COL_WHITE  30,  30,  30   // safe low-power white
#define COL_PURPLE 148,  0, 211
#define COL_GREEN    0, 200,  50
#define COL_RED    210,   0,   0
#define COL_BLUE     0,   0, 255

// ── Tone ──────────────────────────────────────────────────────
#define TONE_BUZZIN 2500

// ── NeoPixel ──────────────────────────────────────────────────
Adafruit_NeoPixel leds[6] = {
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[4], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[5], NEO_GRB + NEO_KHZ800),
};

// ── WebSocket ─────────────────────────────────────────────────
WebSocketsServer webSocket(81);

// ── Game State ────────────────────────────────────────────────
enum GameState { IDLE, STARTED, BUZZED, CORRECT, WRONG };
GameState gameState = IDLE;
int       winnerIdx = -1;

/*
 *  disabled[i] = operator pressed "Turn OFF" for player i
 *                → LED is OFF, button is ignored
 *                → NEVER reset by start_round or reset
 *                → Only cleared by "unlock_player" command
 *
 *  locked[i]   = temporarily locked mid-round (e.g., after
 *                first buzz so others can't press)
 *                → Reset every start_round
 *                → Does NOT affect LED if player is disabled
 */
bool disabled[6] = { false };
bool locked[6]   = { false };

// ── Connection State ──────────────────────────────────────────
bool          appConnected   = false;
unsigned long redBlinkMs     = 0;
bool          redBlinkPhase  = false;
bool          blueFlashing   = false;
int           blueFlashCount = 0;
unsigned long blueFlashMs    = 0;
bool          blueFlashPhase = false;

// ── Button Debounce ───────────────────────────────────────────
unsigned long debounceTimer[6] = { 0 };
bool          lastRead[6]      = { HIGH };
bool          btnState[6]      = { HIGH };

// ── Forward Declarations ──────────────────────────────────────
void setLED(int i, uint8_t r, uint8_t g, uint8_t b);
void setLEDSafe(int i, uint8_t r, uint8_t g, uint8_t b);
void sendEvent(const char* event, int player);
void sendStatusToClient(uint8_t clientNum);
void applyGameStateLEDs();

// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);

  for (int i = 0; i < 6; i++) pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 6; i++) {
    leds[i].begin();
    leds[i].setBrightness(80);
    setLED(i, COL_OFF);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, AP_PASSWORD);

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  Serial.println("BuzzerSystem AP started — 192.168.4.1:81");
}

// ═════════════════════════════════════════════════════════════
void loop() {
  webSocket.loop();

  if (!appConnected) {
    blinkAllRed();
    return;
  }

  if (blueFlashing) {
    handleBlueFlash();
    return;
  }

  readButtons();
}

// ── LED Helpers ───────────────────────────────────────────────

// setLED: raw, always sets regardless of disabled state
void setLED(int i, uint8_t r, uint8_t g, uint8_t b) {
  for (int j = 0; j < NUM_LEDS; j++)
    leds[i].setPixelColor(j, leds[i].Color(r, g, b));
  leds[i].show();
}

// setLEDSafe: respects disabled[] — if player is disabled, forces OFF
void setLEDSafe(int i, uint8_t r, uint8_t g, uint8_t b) {
  if (disabled[i]) {
    setLED(i, COL_OFF);
  } else {
    setLED(i, r, g, b);
  }
}

void ledOff(int i) { setLED(i, COL_OFF); }

// allLEDsOff: turns off ALL including disabled (for idle/reset state)
void allLEDsOff() {
  for (int i = 0; i < 6; i++) setLED(i, COL_OFF);
}

// allLEDsWhite: only lights players who are NOT disabled
void allLEDsWhite() {
  for (int i = 0; i < 6; i++) setLEDSafe(i, COL_WHITE);
}

// othersWhite: set non-winner players to white, skip disabled ones
void othersWhite(int winnerI) {
  for (int i = 0; i < 6; i++) {
    if (i != winnerI) setLEDSafe(i, COL_WHITE);
  }
}

// applyGameStateLEDs: restore LEDs to match state (used on reconnect)
void applyGameStateLEDs() {
  switch (gameState) {
    case IDLE:
      allLEDsOff();
      break;
    case STARTED:
      allLEDsWhite();
      break;
    case BUZZED:
      othersWhite(winnerIdx);
      setLEDSafe(winnerIdx, COL_PURPLE);
      break;
    case CORRECT:
      othersWhite(winnerIdx);
      setLEDSafe(winnerIdx, COL_GREEN);
      break;
    case WRONG:
      othersWhite(winnerIdx);
      setLEDSafe(winnerIdx, COL_RED);
      break;
  }
}

// ── Red Blink (no app connected) ──────────────────────────────
void blinkAllRed() {
  if (millis() - redBlinkMs < 500) return;
  redBlinkMs    = millis();
  redBlinkPhase = !redBlinkPhase;
  for (int i = 0; i < 6; i++) {
    // Even while blinking, respect disabled players — keep them OFF
    if (redBlinkPhase && !disabled[i]) setLED(i, COL_RED);
    else                               setLED(i, COL_OFF);
  }
}

// ── Blue Flash x2 (app connects) ──────────────────────────────
void handleBlueFlash() {
  if (millis() - blueFlashMs < 250) return;
  blueFlashMs    = millis();
  blueFlashPhase = !blueFlashPhase;

  if (blueFlashPhase) {
    // Flash only non-disabled players blue
    for (int i = 0; i < 6; i++) setLEDSafe(i, COL_BLUE);
  } else {
    allLEDsOff();
    blueFlashCount++;
    if (blueFlashCount >= 2) {
      blueFlashing   = false;
      blueFlashCount = 0;
      applyGameStateLEDs();
    }
  }
}

// ── Button Reading ────────────────────────────────────────────
void readButtons() {
  for (int i = 0; i < 6; i++) {
    bool reading = digitalRead(BUTTON_PINS[i]);
    if (reading != lastRead[i]) debounceTimer[i] = millis();
    if (millis() - debounceTimer[i] > 50) {
      if (reading != btnState[i]) {
        btnState[i] = reading;
        if (btnState[i] == LOW) onButtonPress(i);
      }
    }
    lastRead[i] = reading;
  }
}

// ── Button Pressed ────────────────────────────────────────────
void onButtonPress(int idx) {
  if (gameState != STARTED) return;
  if (disabled[idx])        return;   // operator disabled this player
  if (locked[idx])          return;   // round-locked

  // Lock all players for this round
  for (int i = 0; i < 6; i++) locked[i] = true;
  gameState = BUZZED;
  winnerIdx = idx;

  othersWhite(idx);              // others white (skips disabled ones)
  setLED(idx, COL_PURPLE);       // winner always gets color (not disabled)

  tone(BUZZER_PIN, TONE_BUZZIN, 300);
  sendEvent("buzzed_in", idx);
}

// ═════════════════════════════════════════════════════════════
//  GAME COMMANDS
// ═════════════════════════════════════════════════════════════

void cmdStartRound() {
  gameState = STARTED;
  winnerIdx = -1;
  // Reset round locks — but KEEP disabled[] untouched!
  for (int i = 0; i < 6; i++) locked[i] = false;
  allLEDsWhite();  // only lights non-disabled players
  sendEvent("round_started", -1);
}

void cmdReset() {
  gameState = IDLE;
  winnerIdx = -1;
  // Reset round locks — but KEEP disabled[] untouched!
  for (int i = 0; i < 6; i++) locked[i] = false;
  allLEDsOff();
  sendEvent("reset", -1);
}

void cmdCorrect() {
  if (winnerIdx < 0) return;
  gameState = CORRECT;
  setLED(winnerIdx, COL_GREEN);
  othersWhite(winnerIdx);  // skips disabled players
  sendEvent("correct", winnerIdx);
}

void cmdWrong() {
  if (winnerIdx < 0) return;
  gameState = WRONG;
  setLED(winnerIdx, COL_RED);
  othersWhite(winnerIdx);  // skips disabled players
  sendEvent("wrong", winnerIdx);
}

// PASS: mark current player red, unlock others so next person can buzz
void cmdPass(int wrongIdx) {
  if (wrongIdx < 0 || wrongIdx > 5) return;
  setLED(wrongIdx, COL_RED);
  locked[wrongIdx] = true;   // keep this one locked out

  // Unlock all non-disabled, non-wrong players and restore to white
  for (int i = 0; i < 6; i++) {
    if (i != wrongIdx && locked[i] && !disabled[i]) {
      locked[i] = false;
      setLED(i, COL_WHITE);
    }
  }
  gameState = STARTED;
  winnerIdx = -1;
  sendEvent("passed", wrongIdx);
}

// TIME UP: all non-disabled buzzers turn red and lock
void cmdTimeUp() {
  gameState = WRONG;
  for (int i = 0; i < 6; i++) {
    locked[i] = true;
    if (!disabled[i]) setLED(i, COL_RED);
    // disabled players stay OFF
  }
  sendEvent("time_up", -1);
}

// SET WINNER: app-driven, used after PASS queue advances
void cmdSetWinner(int idx) {
  if (idx < 0 || idx > 5) return;
  if (disabled[idx]) return;
  winnerIdx = idx;
  gameState = BUZZED;
  setLED(idx, COL_PURPLE);
  sendEvent("buzzed_in", idx);
}

// LOCK PLAYER (operator "Turn OFF"):
//  - Sets disabled flag (persists across rounds)
//  - Clears round-lock too
//  - Turns LED off immediately
void cmdLockPlayer(int idx) {
  if (idx < 0 || idx > 5) return;
  disabled[idx] = true;
  locked[idx]   = true;
  setLED(idx, COL_OFF);   // force LED off regardless of game state
  sendEvent("player_locked", idx);
}

// UNLOCK PLAYER (operator "Turn ON"):
//  - Clears disabled flag
//  - Clears round-lock
//  - Restore LED to current game state color
void cmdUnlockPlayer(int idx) {
  if (idx < 0 || idx > 5) return;
  disabled[idx] = false;
  locked[idx]   = false;

  // Restore LED to what the current game state expects for this player
  switch (gameState) {
    case IDLE:    setLED(idx, COL_OFF);   break;
    case STARTED: setLED(idx, COL_WHITE); break;
    case BUZZED:
    case CORRECT:
    case WRONG:
      // If this player is the winner, restore their color; else white
      if (idx == winnerIdx) {
        if      (gameState == BUZZED)  setLED(idx, COL_PURPLE);
        else if (gameState == CORRECT) setLED(idx, COL_GREEN);
        else                           setLED(idx, COL_RED);
      } else {
        setLED(idx, COL_WHITE);
      }
      break;
  }
  sendEvent("player_unlocked", idx);
}

void cmdSetLED(int idx, uint8_t r, uint8_t g, uint8_t b) {
  if (idx < 0 || idx > 5) return;
  setLEDSafe(idx, r, g, b);
}

void cmdPlayTone(int freq, int dur) { tone(BUZZER_PIN, freq, dur); }

void cmdSetBrightness(int val) {
  val = constrain(val, 0, 255);
  for (int i = 0; i < 6; i++) { leds[i].setBrightness(val); leds[i].show(); }
}

// ═════════════════════════════════════════════════════════════
//  WEBSOCKET
// ═════════════════════════════════════════════════════════════
void onWebSocketEvent(uint8_t clientNum, WStype_t type,
                      uint8_t* payload, size_t length) {
  switch (type) {

    case WStype_CONNECTED:
      appConnected   = true;
      blueFlashing   = true;
      blueFlashCount = 0;
      blueFlashPhase = false;
      sendStatusToClient(clientNum);
      break;

    case WStype_DISCONNECTED:
      appConnected = false;
      blueFlashing = false;
      break;

    case WStype_TEXT: {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, payload, length)) break;
      const char* cmd = doc["cmd"];
      if (!cmd) break;

      if      (strcmp(cmd, "start_round")    == 0) cmdStartRound();
      else if (strcmp(cmd, "reset")          == 0) cmdReset();
      else if (strcmp(cmd, "correct")        == 0) cmdCorrect();
      else if (strcmp(cmd, "wrong")          == 0) cmdWrong();
      else if (strcmp(cmd, "pass")           == 0) cmdPass(doc["player"].as<int>());
      else if (strcmp(cmd, "time_up")        == 0) cmdTimeUp();
      else if (strcmp(cmd, "set_winner")     == 0) cmdSetWinner(doc["player"].as<int>());
      else if (strcmp(cmd, "lock_player")    == 0) cmdLockPlayer(doc["player"].as<int>());
      else if (strcmp(cmd, "unlock_player")  == 0) cmdUnlockPlayer(doc["player"].as<int>());
      else if (strcmp(cmd, "set_led")        == 0) cmdSetLED(doc["player"], doc["r"], doc["g"], doc["b"]);
      else if (strcmp(cmd, "play_tone")      == 0) cmdPlayTone(doc["freq"].as<int>(), doc["dur"].as<int>());
      else if (strcmp(cmd, "set_brightness") == 0) cmdSetBrightness(doc["value"].as<int>());
      break;
    }
    default: break;
  }
}

// ── Event Broadcaster ─────────────────────────────────────────
void sendEvent(const char* event, int player) {
  StaticJsonDocument<128> doc;
  doc["event"]  = event;
  doc["player"] = player;
  doc["state"]  = (int)gameState;
  char buf[128];
  serializeJson(doc, buf);
  webSocket.broadcastTXT(buf);
}

// ── Status to newly connected client ──────────────────────────
void sendStatusToClient(uint8_t clientNum) {
  StaticJsonDocument<256> doc;
  doc["event"]  = "status";
  doc["state"]  = (int)gameState;
  doc["winner"] = winnerIdx;
  JsonArray arrL = doc.createNestedArray("locked");
  JsonArray arrD = doc.createNestedArray("disabled");
  for (int i = 0; i < 6; i++) {
    arrL.add(locked[i]);
    arrD.add(disabled[i]);
  }
  char buf[256];
  serializeJson(doc, buf);
  webSocket.sendTXT(clientNum, buf);
}
