#!/bin/bash
# helper script to compile and upload the GalaxyRVR sketch via arduino-cli
# usage: ./upload_car.sh <port>
# example: ./upload_car.sh /dev/cu.usbserial-14230

set -e

# location of the sketch
dir=$(dirname "$0")/galaxy-rvr-main/galaxy-rvr
sketch="$dir/galaxy-rvr.ino"

# fully qualified board name
fqbn="arduino:avr:uno"

# if user didn't supply a port try to auto-detect a single available Arduino port
if [ -z "$1" ]; then
  ports=$(arduino-cli board list | awk '/\//{print $1}')
  count=$(echo "$ports" | grep -c '^' || true)
  if [ "$count" -eq 1 ]; then
    port="$ports"
    echo "Using detected port: $port"
  else
    echo "
No serial port provided.  Run 'arduino-cli board list' to see available ports."
    exit 1
  fi
else
  port="$1"
fi

# ensure required libraries are present
echo "Ensuring libraries are installed..."
arduino-cli lib install SoftPWM || true
# SunFounder_AI_Camera isn't on the default index; try git if normal install fails
if ! arduino-cli lib install SunFounder_AI_Camera 2>/dev/null; then
  echo "Installing SunFounder_AI_Camera from GitHub..."
  arduino-cli lib install --git-url https://github.com/sunfounder/SunFounder_AI_Camera.git || true
fi
arduino-cli lib install Servo || true
arduino-cli lib install ArduinoJson || true

echo "Compiling sketch..."
arduino-cli compile --fqbn "$fqbn" "$sketch"

echo "Uploading to $port..."
arduino-cli upload -p "$port" --fqbn "$fqbn" "$sketch"

echo "Done."
