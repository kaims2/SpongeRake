# HackIllinois2026

A modified GalaxyRVR rover project built at HackIllinois 2026.

## Prerequisites

- [Arduino UNO](https://store.arduino.cc/products/arduino-uno-rev3) (or compatible board)
- USB cable to connect the Arduino to your computer
- SunFounder GalaxyRVR kit with ESP32-CAM

## Setup

### 1. Install Arduino CLI

**Windows (winget):**

```bash
winget install ArduinoSA.CLI
```

After installing, restart your terminal or refresh your PATH:

```powershell
$env:Path += ";C:\Program Files\Arduino CLI"
```

### 2. Install the Arduino AVR Core

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
```

### 3. Install Dependency Libraries

```bash
arduino-cli lib install "IRLremote" "SoftPWM" "Servo" "ArduinoJson" "SunFounder AI Camera"
```

## Usage

### Detect your board

Plug in the Arduino UNO via USB and run:

```bash
arduino-cli board list
```

Note the port (e.g. `COM3` on Windows).

### Compile a sketch

```bash
arduino-cli compile --fqbn arduino:avr:uno "galaxy-rvr-main/galaxy-rvr-main/lesson_codes/9_rgb_light_up/9_rgb_light_up.ino"
```

### Compile and upload to the Arduino

> **Note:** Disconnect the ESP32-CAM serial connection before uploading, otherwise you will get sync errors.

```bash
arduino-cli compile --fqbn arduino:avr:uno -p COM3 -u "galaxy-rvr-main/galaxy-rvr-main/lesson_codes/9_rgb_light_up/9_rgb_light_up.ino"
```

Replace `COM3` with your actual port.

### Upload the full GalaxyRVR firmware

```bash
arduino-cli compile --fqbn arduino:avr:uno -p COM3 -u "galaxy-rvr-main/galaxy-rvr-main/galaxy-rvr/galaxy-rvr.ino"
```

After uploading, connect to the rover's WiFi (`GalaxyRVR` / password `12345678`) from the SunFounder Controller app to drive the rover.

### Monitor serial output

For sketches that print sensor data:

```bash
arduino-cli monitor -p COM3 -c baudrate=115200
```

Press `Ctrl+C` to stop.

## Project Structure

- `galaxy-rvr-main/` — SunFounder starter code for the GalaxyRVR
  - `galaxy-rvr/` — Full rover firmware (motor control, sensors, app communication)
  - `lesson_codes/` — Standalone example sketches for learning individual subsystems
  - `test/` — Hardware test sketches

## Contributors

Kai Shah, Cody Olson, Linus Ringstad, Urim Song Zhu
