# WiFiGuard

ESP32-based handheld WiFi safety and network analysis device. Targets Arduino IDE with LilyGO-style board (ST7789 135x240 display, 2 buttons).

## Hardware

- **Board**: LilyGO ESP32 (or compatible) with 1.14" ST7789V IPS LCD (135x240), 4-wire SPI
- **Input**: Two hardware buttons (default pins 0 and 35)
- **Power**: Battery + USB; optional battery ADC on pin 34

## Arduino IDE Setup

1. Install **ESP32** board support (Board Manager: "esp32" by Espressif).
2. Install **TFT_eSPI** (Library Manager).
3. Configure TFT_eSPI for your display:
   - Open `TFT_eSPI/User_Setup.h` in the TFT_eSPI library folder.
   - Select your driver (ST7789) and resolution 135x240.
   - Set correct SPI pins for your LilyGO board (e.g. TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK).
   - Or copy a LilyGO-specific setup if available.
4. Open `WiFiGuard/WiFiGuard.ino` in Arduino IDE.
5. Select board: **ESP32 Dev Module** (or your LilyGO board if present).
6. Compile and upload.

## Pins (Config.h)

- `PIN_BTN1`, `PIN_BTN2`: button GPIOs (default 0, 35)
- `PIN_BATTERY_ADC`: battery voltage ADC (default 34)
- `PIN_TFT_BL`: display backlight (default 27)

Adjust in `Config.h` for your hardware.

## Usage

- **Long-press B1** (from idle): Start WiFi scan.
- **B1 tap**: Next item in list.
- **B2 tap**: Previous item.
- **Long-press B1** (on list): Select network — open detail or run connectivity test (open networks only).
- **Long-press B2** (on list): Environment summary → Settings.
- **Chord** (both buttons): Export current scan to Serial (CSV).
- **Settings**: B1 = cycle sort mode, B2 = toggle filter; Long B2 = save and back.

## Serial Export

Connect over USB Serial (115200 baud). Trigger export with chord. CSV columns: scanIndex, networkIndex, SSID, BSSID (masked by default), RSSI, auth, channel, risk, grade, portal, hidden, duplicate.

## Architecture

- **Config.h / Types.h**: Build config and data types.
- **StateMachine**: Idle, Scanning, Processing, Browsing, Testing, Export, Settings, Sleep.
- **WiFiScanner**: Async scan, dedupe, duplicate-SSID marking.
- **RiskEngine**: 0–100 risk score and reason bitmask per network.
- **EnvironmentAnalysis**: Channel histogram, congestion, summary.
- **ConnectivityTest**: Non-blocking associate / DNS / HTTP; portal detection.
- **ScanHistory**: Circular buffer; optional NVS persist.
- **Settings**: NVS-backed sort/filter/privacy options.
- **PowerManager**: Battery read, inactivity timeout, sleep.
