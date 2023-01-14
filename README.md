|Example(Chip)/Platform   | Arduino  | PlatformIO  |
|---|---|---|
| Esp8266  | [![CI-Arduino-Esp8266](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-arduino-esp8266.yml/badge.svg)](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-arduino-esp8266.yml)  | [![CI-PlatformIO-Esp8266](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-platformio-esp8266.yml/badge.svg)](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-platformio-esp8266.yml)  |
|  Esp32 | [![CI-Arduino-Esp32](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-arduino-esp32.yml/badge.svg)](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-arduino-esp32.yml)  | [![CI-PlatformIO-Esp32](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-platformio-esp32.yml/badge.svg)](https://github.com/axcap/Esp-GitHub-OTA/actions/workflows/ci-platformio-esp32.yml)  |

# Esp-GitHub-OTA

Inspired by [esp_ghota](https://github.com/Fishwaldo/esp_ghota) by **Fishwaldo**

Brings same functionality to Arduino platform

# How to get started
1. Create github repository and clone it locally (**root_dir**)
2. Download this repository by pressing Code -> Download ZIP and install it via Arduino IDE (Sketch -> Include Library -> Add .Zip Library)
3. Open one of the examples provided with this library and save it to the newly cloned directory
4. Replace VERSION (line 7) and RELEASE_URL (line 11) with your values
5. Additionally clone following files from this repo as Arduino does not do it for us
    * platformio.ini
    * .gitignore
    * .github (this folder contains GitHub Actions that will create releases of your OTA firmware)
6. Commit and push all staged files (run 'git add -am "First commit" && git push' from terminal from **root_dir** (see step 1))
7. Github actions will trigger automatically to check for any errors in the code, but no release will be created yet
8. When you are happy with your code create git tag by runnig 'git tag v0.0.1' for version 0.0.1
9. Push your tag with 'git push origin v0.0.1', this will trigger Release creation
10. Update DELAY_MS (line 13) to 100 and VERSION (line 7) to 0.0.2
11. Repeat steps 5-8 to create new release and wait for your microcontroller to pick it up
12. Led should toggle much faster 🚨

This library is under development and may not work every time. <br />
Feel free to open Issues or create Pull requests :) 
