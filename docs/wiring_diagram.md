# Wiring Diagram — Quiz Buzzer System

## Components Needed

### Per Buzzer Unit (×6)
| # | Component | Specs |
|---|-----------|-------|
| 1 | Push Button | Momentary, any color |
| 1 | WS2812B LED | NeoPixel, 5V (addressable RGB) |
| 1 | 470Ω resistor | For LED data line |
| 1 | 100µF capacitor | For LED power stabilization |

### Central Unit (×1)
| # | Component | Specs |
|---|-----------|-------|
| 1 | ESP32 Dev Board | 38-pin, any variant |
| 1 | Passive Buzzer | 3.3V or 5V compatible |
| 1 | USB Power Supply | 5V 2A min (or battery bank) |

---

## ESP32 Pin Assignments

### Button Inputs (connect to GND via button)
| GPIO Pin | Player # |
|----------|----------|
| GPIO 13  | Player 1 |
| GPIO 12  | Player 2 |
| GPIO 14  | Player 3 |
| GPIO 27  | Player 4 |
| GPIO 26  | Player 5 |
| GPIO 25  | Player 6 |

> All buttons use INTERNAL PULL-UP.
> Wire one side of each button to the GPIO pin.
> Wire the other side to GND.

### LED Data Line (WS2812B NeoPixel, chained)
| GPIO Pin | Purpose |
|----------|---------|
| GPIO 5   | NeoPixel DATA IN (chain all 6 LEDs) |

> Chain the LEDs: ESP32 GPIO5 → LED1 DATA IN → LED1 DATA OUT → LED2 DATA IN → ... → LED6
> Power all LEDs from the 5V pin (or external 5V).
> Add a 470Ω resistor on the DATA line.
> Add a 100µF capacitor between 5V and GND at the first LED.

### Buzzer (Passive/Piezo)
| GPIO Pin | Purpose |
|----------|---------|
| GPIO 4   | Passive Buzzer + pin |

> Connect buzzer + to GPIO 4.
> Connect buzzer - to GND.

---

## Wiring Diagram (Text)

```
ESP32
                                 +5V ──── [100µF cap] ──── GND
                                  |
GPIO 5 ── [470Ω] ──── LED1 DIN ──┤
                       LED1 DOUT ─── LED2 DIN
                                     LED2 DOUT ─── LED3 DIN
                                                    LED3 DOUT ─── LED4 DIN
                                                                   ...
                                                                   LED6 DOUT (end)

GPIO 4  ──── Buzzer(+)
GND     ──── Buzzer(-)

GPIO 13 ──── [Button 1] ──── GND     (Player 1)
GPIO 12 ──── [Button 2] ──── GND     (Player 2)
GPIO 14 ──── [Button 3] ──── GND     (Player 3)
GPIO 27 ──── [Button 4] ──── GND     (Player 4)
GPIO 26 ──── [Button 5] ──── GND     (Player 5)
GPIO 25 ──── [Button 6] ──── GND     (Player 6)
```

---

## Physical Layout Suggestion

Each "buzzer unit" is a small box with:
- Big push button on top (arcade-style recommended)
- WS2812B LED glowing through a diffuser or the button itself (many arcade buttons have built-in LEDs — check if WS2812B fits your button)
- Wire going back to central ESP32 unit (4 wires: VCC, GND, DATA, button signal)

Cable per unit: **4 wires × up to 3 meters** (use ribbon cable or 4-core wire)

---

## Parts List (Shopping Guide)

| Item | Qty | Approx Cost |
|------|-----|-------------|
| ESP32 Dev Board | 1 | ~$5–10 |
| WS2812B NeoPixel LED (individual) | 6 | ~$3 |
| Arcade push buttons (60mm, round) | 6 | ~$12 |
| Passive piezo buzzer | 1 | ~$1 |
| 470Ω resistors | 6 | \$0.50 |
| 100µF capacitors | 2 | \$0.50 |
| Ribbon cable (4-wire, 3m each) | 6 | ~$6 |
| Small project box / 3D print housing | 7 | varies |
| 5V 2A USB power supply | 1 | ~$5 |

**Total estimated hardware cost: ~\$33–50**
