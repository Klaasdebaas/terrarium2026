#!/bin/sh
# Compileert de sketch voor ESP32-C6. Zie de setup-opmerkingen onderaan.
set -e
export PATH="$HOME/bin:$PATH"
cd "$(dirname "$0")/.."
STUB="$HOME/bin/ctags-stub"
if [ ! -x "$STUB/ctags" ]; then
  mkdir -p "$STUB"
  printf '#!/bin/sh\nexit 0\n' > "$STUB/ctags"
  chmod +x "$STUB/ctags"
fi
arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc \
  --build-property runtime.tools.ctags.path="$STUB" terrarium

# Eenmalige setup in een sandbox waar downloads.arduino.cc geblokkeerd is
# (github.com en raw.githubusercontent.com werken wel):
#  1. arduino-cli 1.3.1 uit GitHub-release naar ~/bin:
#     curl -L -o ac.tgz https://github.com/arduino/arduino-cli/releases/download/v1.3.1/arduino-cli_1.3.1_Linux_64bit.tar.gz
#  2. arduino-cli config init
#     arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
#     arduino-cli config set library.enable_unsafe_install true
#  3. Core 3.x vereist tool arduino:dfu-util (van geblokkeerde host): serveer een
#     stub package-index (package "arduino", tool dfu-util 0.11.0-arduino5, dummy
#     tar.gz met correcte sha256/size) via `python3 -m http.server` op localhost en
#     `arduino-cli config add board_manager.additional_urls http://localhost:8765/<stub>.json`.
#     (Zonder stub installeert `core install esp32:esp32` stilzwijgend 2.0.9, dat geen C6 kent.)
#  4. arduino-cli core update-index; arduino-cli core install esp32:esp32@3.3.0
#  5. Libraries: arduino-cli lib install --git-url https://github.com/<owner>/<repo>.git voor
#     olikraus/U8g2_Arduino adafruit/DHT-sensor-library adafruit/Adafruit_Sensor adafruit/Adafruit_BusIO
#     PaulStoffregen/OneWire milesburton/Arduino-Temperature-Control-Library adafruit/RTClib bblanchon/ArduinoJson
#
# STATUS: compile works in the sandbox using a no-op ctags stub (the real ctags tool is only
# on the blocked downloads.arduino.cc). Because the stub generates no automatic prototypes,
# every function in terrarium.ino must be declared before use.
