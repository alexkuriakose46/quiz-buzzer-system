# 🎯 Quiz Buzzer System

A professional, wireless 6-player quiz buzzer system built with an ESP32 microcontroller and a Python desktop application.

---

## 📦 Project Structure

```
quiz-buzzer-system/
├── app/
│   └── main.py              # Python Desktop Controller App
├── firmware/
│   └── buzzer_system/
│       └── buzzer_system.ino  # ESP32 Arduino Firmware
└── README.md
```

---

## ⚙️ How It Works

- The **ESP32** creates its own Wi-Fi Access Point (`BuzzerSystem` / `buzzer123`)
- The **Desktop App** connects to `192.168.4.1` via WebSocket (Port 81)
- All 6 player boxes have a push button + 10-LED WS2812B strip
- A single external buzzer is connected to GPIO 32

---

## 🔌 Hardware Wiring

| Player | LED Data (Yellow) | Button Signal (Green) |
|--------|-------------------|-----------------------|
| 1      | GPIO 5            | GPIO 13               |
| 2      | GPIO 18           | GPIO 12               |
| 3      | GPIO 19           | GPIO 14               |
| 4      | GPIO 21           | GPIO 27               |
| 5      | GPIO 22           | GPIO 26               |
| 6      | GPIO 23           | GPIO 25               |

- **5V / VIN** → Red wire (LED power)
- **GND** → Black wire (shared ground)
- **External Buzzer** → GPIO 32

---

## 🎮 Game LED Colors

| State | Color |
|-------|-------|
| No App Connected | 🔴 Red blinking |
| App Connected | 🔵 Blue flash x2 |
| Game Started | ⬜ Solid White |
| Buzzed In (Winner) | 🟣 Purple |
| Correct Answer | 🟢 Green |
| Wrong Answer / Time Up | 🔴 Red |

---

## 🖥️ Desktop App Features

- **Game Timer** — starts fresh on every round, stops with ⏹ button
- **Time Limit** — toggle ON/OFF, set custom seconds, auto-stops round when time runs out
- **Activity Log** — records every buzz with player name and exact timestamp
- **PASS Button** — marks current player red, unlocks others for next buzz
- **Laptop Sound** — loud beep from laptop speakers when someone buzzes in
- **Correct / Wrong** — judge buttons with green/red feedback on hardware LEDs

---

## 📋 App Requirements

```bash
pip install customtkinter websockets
```

Run with:
```bash
python app/main.py
```

Or use the prebuilt `QuizBuzzer.exe` (Windows only).

---

## 🔧 Firmware Libraries (Arduino IDE)

- `Adafruit NeoPixel`
- `WebSockets` by Markus Sattler
- `ArduinoJson`

Board: **ESP32 Dev Module**
