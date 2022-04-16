![icon](https://user-images.githubusercontent.com/61933721/123552174-983dc180-d77d-11eb-9d72-8daecaa46584.png)

# ESP32-INMP441-PxMATRIX-SPECTRUM
ESP32 spectrum analyzer with INMP441 I2S microphone and a space arcade game with bluetooth control via any Android device.

![Spectrum](https://raw.githubusercontent.com/jozsefcsiza/Esp32-PxMatrix-Spectrum/main/Spectrum1.gif)
![Spectrum](https://raw.githubusercontent.com/jozsefcsiza/Esp32-PxMatrix-Spectrum/main/Spectrum2.gif)
![Spectrum](https://raw.githubusercontent.com/jozsefcsiza/Esp32-PxMatrix-Spectrum/main/Game.gif)

# HARDWARE
Px Led Panel(P2, P2.5, P3, P5, etc...).
First you have to make your led panel to work. To setup and troubleshoot visit the following link https://github.com/2dom/PxMatrix

INMP441 I2S microphone.

ESP32 dev board, the exact type doesn't matter.

Any android devce with bluetooth.

# WIRING

![Wiring](https://user-images.githubusercontent.com/61933721/142764859-445b5130-671d-430f-9cb5-2aa01c29e51b.png)

# REQUIRED LIBRARIES
Esp32 1.0.4 -> Board manager library -> Use only the 1.0.4 version, with higher versions will not work the audio process

FastLED -> Arduino libraries manager -> This library is used for color manipulation

ArduinoFFT -> Arduino libraries manager -> This library is used for audio process

BluetoothSerial -> Built in -> This library is used for ESP32 and Android device comunication via bluetooth

Preferences -> Built in -> This library is used to save some preferences in the EPS32 memory

PxMatrix -> Included in the project and is used to control a Px matrix panel	https://github.com/2dom/PxMatrix visit this link to setup and troubleshoot your Px matrix panel!!!!!!

Adafruit GFX -> Arduino libraries manager -> This library is needed for the PxMatrix header

Adafruit BusIO -> Arduino libraries manager -> This library is needed for the PxMatrix header
  
# BUILD AND RUN
Wire your ESP32 properly.

Upload the arduino code to your ESP32 device.

Install the Esp32.apk tou your android device.

![Spectrum2](https://raw.githubusercontent.com/jozsefcsiza/Esp32-PxMatrix-Spectrum/main/MainMenu.png)

SPECTRUM SETTINGS - Accesss the spectrum settings.

SPACE ARCADE GAME - Start the arcade game.

SELECT BLUETOOTH - Select the esp32 bluetooth, it is saved and you don't have to reselect all the time.

ESP32 DEEP SLEEP - Pauses the audio process and sets the led brightness to 0, so only the bluetooth will remain in function and after restarting the android app the spectrum analyzer will be activated automatically. It is like a switch off, except the bluetooth.

![Spectrum1](https://raw.githubusercontent.com/jozsefcsiza/Esp32-PxMatrix-Spectrum/main/SpectrumSettings.png)

NEXT PATTERN - You can switch between the built in patterns.

SMOOTH PATTERN - You can enable or disable the smooth pattern.

BRIGHTNESS - Led brightness setting. It consumes amps. If you use it with PC or something, at maximum brightness can freeze the esp. A phone charger has enough amps to use it on maximum brightness.

GAIN - Increases the sensitivity of the microphone to adjust for louder or quieter environoments.

SQUELCH - Increasing this value puts a limit on the quietest sounds that will be picked up. Useful for if you have some background noise to remove.

RESET (DEFAULTS) - Resets all the spectrum settings.
