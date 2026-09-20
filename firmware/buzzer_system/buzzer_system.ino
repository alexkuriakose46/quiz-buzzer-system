/*
 * ============================================================
 *  Quiz Buzzer System — ESP32 Firmware  (FINAL V3)
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

// ── NeoPixel Config ───────────────────────────────────────────
#define NUM_LEDS 10

// ── Colors ────────────────────────────────────────────────────
#define COL_OFF     0,   0,   0
#define COL_WHITE   30,  30,  30  // VERY LOW POWER TEST
#define COL_PURPLE  148,   0, 211
#define COL_GREEN     0, 210,  50
#define COL_RED     210,   0,   0
#define COL_BLUE      0,   0, 255

// ── Tones ─────────────────────────────────────────────────────
#define TONE_BUZZIN   880

// ── NeoPixel ─────────────────────────────────────────────────
Adafruit_NeoPixel leds[6] = {
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[4], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, LED_PINS[5], NEO_GRB + NEO_KHZ800),
};

// ── WebSocket ────────────────────────────────────────────────
WebSocketsServer webSocket(81);

// ── Game State ───────────────────────────────────────────────
enum GameState { IDLE, STARTED, BUZZED, CORRECT, WRONG };
GameState gameState = IDLE;
int       winnerIdx = -1;
bool      locked[6] = { false };

// ── Connection State ─────────────────────────────────────────
bool          appConnected   = false;
unsigned long redBlinkMs     = 0;
bool          redBlinkPhase  = false;
bool          blueFlashing   = false;
int           blueFlashCount = 0;
unsigned long blueFlashMs    = 0;
bool          blueFlashPhase = false;

// ── Button Debounce ──────────────────────────────────────────
unsigned long debounceTimer[6] = { 0 };
bool          lastRead[6]      = { HIGH };
bool          btnState[6]      = { HIGH };

// ── Forward Declarations ──────────────────────────────────────
void setLED(int i, uint8_t r, uint8_t g, uint8_t b);
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
    leds[i].setBrightness(80); // Good brightness, safe power draw
    setLED(i, COL_OFF);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, AP_PASSWORD);

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
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
void setLED(int i, uint8_t r, uint8_t g, uint8_t b) {
  for (int j = 0; j < NUM_LEDS; j++) {
    leds[i].setPixelColor(j, leds[i].Color(r, g, b));
  }
  leds[i].show();
}

void ledOff(int i) { setLED(i, COL_OFF); }

void allLEDsOff() {
  for (int i = 0; i < 6; i++) ledOff(i);
}

void allLEDsWhite() {
  for (int i = 0; i < 6; i++) setLED(i, COL_WHITE);
}

void othersWhite(int winnerI) {
  for (int i = 0; i < 6; i++) {
    if (i != winnerI) setLED(i, COL_WHITE);
  }
}

// ── Restore LEDs to match current game state ──────────────────
void applyGameStateLEDs() {
  if (gameState == IDLE) {
    allLEDsOff();
  } else if (gameState == STARTED) {
    allLEDsWhite();
  } else if (gameState == BUZZED) {
    othersWhite(winnerIdx);
    setLED(winnerIdx, COL_PURPLE);
  } else if (gameState == CORRECT) {
    othersWhite(winnerIdx);
    setLED(winnerIdx, COL_GREEN);
  } else if (gameState == WRONG) {
    othersWhite(winnerIdx);
    setLED(winnerIdx, COL_RED);
  }
}

// ── Red Blink (no app) ────────────────────────────────────────
void blinkAllRed() {
  if (millis() - redBlinkMs < 500) return;
  redBlinkMs    = millis();
  redBlinkPhase = !redBlinkPhase;
  for (int i = 0; i < 6; i++) {
    if (redBlinkPhase) setLED(i, COL_RED);
    else               setLED(i, COL_OFF);
  }
}

// ── Blue Flash x2 (app connects) ──────────────────────────────
void handleBlueFlash() {
  if (millis() - blueFlashMs < 250) return;
  blueFlashMs    = millis();
  blueFlashPhase = !blueFlashPhase;

  if (blueFlashPhase) {
    for (int i = 0; i < 6; i++) setLED(i, COL_BLUE);
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
  if (locked[idx])          return;

  for (int i = 0; i < 6; i++) locked[i] = true;
  gameState = BUZZED;
  winnerIdx = idx;

  // Winner is PURPLE, others stay WHITE
  othersWhite(idx);
  setLED(idx, COL_PURPLE);

  // Play sound ONLY when someone buzzes in
  tone(BUZZER_PIN, TONE_BUZZIN, 300);
  
  sendEvent("buzzed_in", idx);
}

// ═════════════════════════════════════════════════════════════
//  GAME COMMANDS
// ═════════════════════════════════════════════════════════════
void cmdStartRound() {
  gameState = STARTED;
  winnerIdx = -1;
  for (int i = 0; i < 6; i++) locked[i] = false;
  
  allLEDsWhite(); // Solid white, no blinking
  
  // NO SOUND
  sendEvent("round_started", -1);
}

void cmdReset() {
  gameState = IDLE;
  winnerIdx = -1;
  for (int i = 0; i < 6; i++) locked[i] = false;
  allLEDsOff();
  // NO SOUND
  sendEvent("reset", -1);
}

void cmdCorrect() {
  if (winnerIdx < 0) return;
  gameState = CORRECT;
  
  setLED(winnerIdx, COL_GREEN);
  othersWhite(winnerIdx);
  
  // NO SOUND
  sendEvent("correct", winnerIdx);
}

void cmdWrong() {
  if (winnerIdx < 0) return;
  gameState = WRONG;
  
  setLED(winnerIdx, COL_RED);
  othersWhite(winnerIdx);
  
  // NO SOUND
  sendEvent("wrong", winnerIdx);
}

void cmdLockPlayer(int idx) {
  if (idx < 0 || idx > 5) return;
  locked[idx] = true;
  ledOff(idx);
  sendEvent("player_locked", idx);
}

void cmdUnlockPlayer(int idx) {
  if (idx < 0 || idx > 5) return;
  locked[idx] = false;
  sendEvent("player_unlocked", idx);
}

// PASS: Mark one player red, unlock all others so next person can buzz
void cmdPass(int wrongIdx) {
  if (wrongIdx < 0 || wrongIdx > 5) return;
  setLED(wrongIdx, COL_RED);        // Mark passed player as red
  locked[wrongIdx] = true;          // Keep them locked out
  // Unlock everyone else who was not already out
  for (int i = 0; i < 6; i++) {
    if (i != wrongIdx && locked[i]) {
      locked[i] = false;
      setLED(i, COL_WHITE);         // Restore them to white (ready)
    }
  }
  gameState = STARTED;              // Accept new button presses
  winnerIdx = -1;
  sendEvent("passed", wrongIdx);
}

// TIME UP: All buzzers turn red, all locked, game over
void cmdTimeUp() {
  gameState = WRONG;
  for (int i = 0; i < 6; i++) {
    locked[i] = true;
    setLED(i, COL_RED);
  }
  sendEvent("time_up", -1);
}

// SET WINNER: Used by app to show who is "next" in pass queue
void cmdSetWinner(int idx) {
  if (idx < 0 || idx > 5) return;
  winnerIdx = idx;
  gameState = BUZZED;
  setLED(idx, COL_PURPLE);
  sendEvent("buzzed_in", idx);
}

void cmdSetLED(int idx, uint8_t r, uint8_t g, uint8_t b) {
  if (idx < 0 || idx > 5) return;
  setLED(idx, r, g, b);
}

void cmdPlayTone(int freq, int dur) { tone(BUZZER_PIN, freq, dur); }

void cmdSetBrightness(int val) {
  val = constrain(val, 0, 255);
  for (int i = 0; i < 6; i++) {
    leds[i].setBrightness(val);
    leds[i].show();
  }
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

void sendEvent(const char* event, int player) {
  StaticJsonDocument<128> doc;
  doc["event"]  = event;
  doc["player"] = player;
  doc["state"]  = (int)gameState;
  char buf[128];
  serializeJson(doc, buf);
  webSocket.broadcastTXT(buf);
}

void sendStatusToClient(uint8_t clientNum) {
  StaticJsonDocument<256> doc;
  doc["event"]  = "status";
  doc["state"]  = (int)gameState;
  doc["winner"] = winnerIdx;
  JsonArray arr = doc.createNestedArray("locked");
  for (int i = 0; i < 6; i++) arr.add(locked[i]);
  char buf[256];
  serializeJson(doc, buf);
  webSocket.sendTXT(clientNum, buf);
}
