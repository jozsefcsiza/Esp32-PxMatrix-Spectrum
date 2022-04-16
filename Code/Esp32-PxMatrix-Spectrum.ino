/*
 * Created by Jozsef Csiza 11/21/2021	https://github.com/jozsefcsiza
 * FFT VU meter for a PxMatrix panel(P2, P2.5, P3, etc...), ESP32 and INMP441 digital mic and bluetooth control via any android device.
 * Matrix resolution can be any -> 32x32, 64x32, 64x64, etc...
 * REQUIRED LIBRARIES
 * Esp32 1.0.4        Board manager library -> Use only the 1.0.4 version, with higher versions will not work the audio process
 * FastLED            Arduino libraries manager -> This library is used for color manipulation
 * ArduinoFFT         Arduino libraries manager -> This library is used for audio process
 * BluetoothSerial    Built in -> This library is used for ESP32 and Android device comunication via bluetooth
 * Preferences		  Built in -> This library is used to save some preferences in the EPS32 memory
 * PxMatrix           Included in the project and is used to control a Px matrix panel	https://github.com/2dom/PxMatrix visit this link to setup and troubleshoot your Px matrix panel!!!!!!
 * Adafruit GFX       Arduino libraries manager -> This library is needed for the PxMatrix header
 * Adafruit BusIO     Arduino libraries manager -> This library is needed for the PxMatrix header
 *
 * Audio and mic processing -> Andrew Tuline et al     https://github.com/atuline/WLED
 */

#define double_buffer

#include "variables.h"
#include <FastLED.h>
#include <PxMatrix.h>
#include "audio_reactive.h"
#include "mybluetooth.h"
#include "game.h"

void IRAM_ATTR display_updater() {
    // Increment the counter and set the time of ISR
    //portENTER_CRITICAL_ISR(&timerMux);
    //display.display(display_draw_time);
    //portEXIT_CRITICAL_ISR(&timerMux);

    display.display(display_draw_time);
}

void display_update_enable(bool is_enable) {
    if (is_enable) {
        timer = timerBegin(0, 80, true);
        timerAttachInterrupt(timer, &display_updater, true);
        timerAlarmWrite(timer, 4000, true);
        timerAlarmEnable(timer);
    } else {
        timerDetachInterrupt(timer);
        timerAlarmDisable(timer);
    }
}

DEFINE_GRADIENT_PALETTE(classic_gp) {
    0, 0, 255, 0,           //green
    128, 255, 165, 0,       //yellow
    255, 255, 0, 0,         //red
};
CRGBPalette16 classicPal = classic_gp;

// the setup function runs once when you press reset or power the board
void setup() {
    Serial.begin(9600);
    startBlueTooth();
    delay(250);

    preferences.begin(ESP_PREFS, false); //The false argument means that we’ll use it in read/write mode
    brightness = preferences.getUInt(PREFS_BRIGHTNESS, 50);
    gain = preferences.getUInt(PREFS_GAIN, 30);
    squelch = preferences.getUInt(PREFS_SQUELCH, 0);
    pattern = preferences.getUInt(PREFS_PATTERN, 1);
    smoothSpectrum = preferences.getUInt(SMOOTH_SPECTRUM, 0);
    highScore = preferences.getUInt(SPACE_GAME_HIGH_SCORE, 0);
    preferences.end();

    //Px matrix panel setup. If you have problems with yours you have to troubleshoot by accessing the following link https://github.com/2dom/PxMatrix
    display.begin(32); //Matrix Step -> 1/4, 1/8, 1/16, 1/32.
    display.setBrightness(0);
    display.flushDisplay();

    display_update_enable(true);

    delay(250);
    setSpectrumBands();
    for (int i = 0; i < NUM_BANDS; i++) {
        peak[i] = 0;
        prevFFTValue[i] = 0;
        barHeights[i] = 0;
    }
    setupAudio();
}

// the loop function runs over and over again until power down or reset
void loop() {
    readBlueTooth();
    if (!isDeepSlepp) {
        if (isSpectrum) {
            spectrumVoid();
        } else if (isGame) {
            playGame();
        }
    }
}

void spectrumVoid()
{
    display.clearDisplay();
    display.setBrightness(brightness);

    uint8_t fftValue;
    for (int i = 0; i < NUM_BANDS; i++) {
        fftValue = fftResult[i];

        fftValue = ((prevFFTValue[i] * 3) + fftValue) / 4;                  // Dirty rolling average between frames to reduce flicker
        barHeights[i] = fftValue / (255 / M_HEIGHT);                        // Scale bar height

        if (barHeights[i] > peak[i])                                        // Move peak up
            peak[i] = min(M_HEIGHT, (int)barHeights[i]);

        prevFFTValue[i] = fftValue;                                         // Save prevFFTValue for averaging later
    }

    drawPattern();

    // Decay peak
    EVERY_N_MILLISECONDS(20) {
        if (isSpectrum)
            for (uint8_t band = 0; band < NUM_BANDS; band++)
                if (peak[band] > 0) peak[band] -= 1;
    }

    display.showBuffer();
    delay(15); //Some delay to avoid flicker
}

void saveSpectrumPrefs() {
    preferences.begin(ESP_PREFS, false); //The false argument means that we’ll use it in read/write mode
    preferences.putUInt(PREFS_BRIGHTNESS, brightness);
    preferences.putUInt(PREFS_GAIN, gain);
    preferences.putUInt(PREFS_SQUELCH, squelch);
    preferences.putUInt(PREFS_PATTERN, pattern);
    preferences.putUInt(SMOOTH_SPECTRUM, smoothSpectrum);
    preferences.end();
}

void deepSlepp() {
    isDeepSlepp = true;
    pauseAudio();
    display.clearDisplay();
    display.setBrightness(0);
    display_update_enable(false);
}

void wakeUp() {
    if (isDeepSlepp) {
        isDeepSlepp = false;
        resumeAudio();
        display_update_enable(true);
    }
}

void drawPattern() {
    switch (pattern) {
    case 0:
        classicBars();
        break;
    case 1:
        rainbowBars();
        break;
    case 2:
        rainbowPeaks();
        break;
    case 3:
        mirrorBars();
        break;
    }
}

void classicBars() {
    uint8_t barWidth = M_WIDTH / NUM_BANDS;
    uint8_t barHeight;
    CRGB color;
    uint16_t peakHeight;
    int colorIndex, counter;
    for (int x = 0; x < NUM_BANDS; x++)
    {
        barHeight = barHeights[x];
        if (barHeight > 0)
        {
            counter = 0;
            for (int y = M_HEIGHT - 1; y >= M_HEIGHT - barHeight; y--) {
                colorIndex = ((M_HEIGHT - y) * counter) / ((M_HEIGHT * M_HEIGHT) / 256);
                color = ColorFromPalette(classicPal, colorIndex);
                for (int j = 0; j < barWidth; j++) {
                    if ((j > 0) || smoothSpectrum == 1)
                    {
                        if (y > 0) {
                            display.drawPixel(x * barWidth + j, y, display.color565(color.red, color.green, color.blue));
                        }
                        else {
                            display.drawPixel(x * barWidth + j, y, display.color565(255, 0, 0));
                        }
                    }
                }
                counter++;
            }
        }
        peakHeight = peak[x];
        if (peakHeight > 0 && peakHeight < M_HEIGHT) {
            for (int j = 0; j < barWidth; j++) {
                if ((j > 0) || smoothSpectrum == 1)
                    display.drawPixel(x * barWidth + j, M_HEIGHT - peakHeight, white);
            }
        }
    }
}

void mirrorBars() {
    uint8_t barWidth = M_WIDTH / NUM_BANDS;
    uint8_t barHeight;
    CRGB rgb;
    CHSV chsv;
    uint16_t sColor, peakHeight;
    for (int x = 0; x < NUM_BANDS; x++)
    {
        barHeight = barHeights[x] / 2;
        chsv = CHSV(x * (255 / NUM_BANDS), 255, 255);
        hsv2rgb_rainbow(chsv, rgb);
        sColor = display.color565(rgb.red, rgb.green, rgb.blue);
        for (int j = 0; j < barWidth; j++)
            if ((j > 0) || smoothSpectrum == 1)
                display.drawPixel(x * barWidth + j, M_HEIGHT / 2, sColor);
        if (barHeight > 0)
        {
            for (int y = M_HEIGHT / 2; y >= M_HEIGHT / 2 - barHeight; y--) {
                if (y >= 0) {
                    for (int j = 0; j < barWidth; j++) {
                        if ((j > 0) || smoothSpectrum == 1)
                            display.drawPixel(x * barWidth + j, y, sColor);
                    }
                }
            }

            for (int y = M_HEIGHT / 2; y <= M_HEIGHT / 2 + barHeight; y++) {
                if (y <= M_HEIGHT - 1) { 
                    for (int j = 0; j < barWidth; j++) {
                        if ((j > 0) || smoothSpectrum == 1)
                            display.drawPixel(x * barWidth + j, y, sColor);
                    }
                }
            }
        }
    }
}

void rainbowBars() {
    uint8_t barWidth = M_WIDTH / NUM_BANDS;
    uint8_t barHeight;
    CRGB rgb;
    CHSV chsv;
    uint16_t sColor, peakHeight;
    for (int x = 0; x < NUM_BANDS; x++)
    {
        barHeight = barHeights[x];
        if (barHeight > 0)
        {
            chsv = CHSV(x * (255 / NUM_BANDS), 255, 255);
            hsv2rgb_rainbow(chsv, rgb);
            sColor = display.color565(rgb.red, rgb.green, rgb.blue);
            for (int y = M_HEIGHT - 1; y >= M_HEIGHT - barHeight; y--) {
                for (int j = 0; j < barWidth; j++) {
                    if ((j > 0) || smoothSpectrum == 1)
                        display.drawPixel(x * barWidth + j, y, sColor);
                }
            }
        }
        peakHeight = peak[x];
        if (peakHeight > 0 && peakHeight < M_HEIGHT) {
            for (int j = 0; j < barWidth; j++) {
                if ((j > 0) || smoothSpectrum == 1)
                    display.drawPixel(x * barWidth + j, M_HEIGHT - peakHeight, white);
            }
        }
    }
}

void rainbowPeaks() {
    uint8_t barWidth = M_WIDTH / NUM_BANDS;
    CRGB rgb;
    CHSV chsv;
    uint16_t sColor, peakHeight;
    for (int x = 0; x < NUM_BANDS; x++)
    {
        peakHeight = peak[x];
        if (peakHeight > 0 && peakHeight < M_HEIGHT) {
            chsv = CHSV(x * (255 / NUM_BANDS), 255, 255);
            hsv2rgb_rainbow(chsv, rgb);
            sColor = display.color565(rgb.red, rgb.green, rgb.blue);
            for (int j = 0; j < barWidth; j++) {
                if ((j > 0) || smoothSpectrum == 1)
                    display.drawPixel(x * barWidth + j, M_HEIGHT - peakHeight, sColor);
            }
        }
    }
}

#pragma region Bluetooth
void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    if (event == ESP_SPP_SRV_OPEN_EVT) {
        Serial.println("Client Connected");
    }
}

void startBlueTooth() {
    SerialBT.register_callback(callback);

    if (!SerialBT.begin(BLUETOOTH_NAME)) {
        Serial.println("An error occurred initializing Bluetooth");
    } else {
        Serial.println("Bluetooth initialized");
    }
}

void getBtValue(char c1, char c2, char c3) {
    try {
        char str[3] = { c1, c2, c3 };
        bluetoothData = atoi(str);
    } catch (const std::exception&) {
        bluetoothData = O;
    }
}

void readBlueTooth() {
    BT_BUTTON = "";
    if (SerialBT.available())
    {
        if (isSpectrum)
            display_update_enable(false); //It is important to disable the display update. If not, the ESP will crash on bluetooth communication!!!
        int i = 0;
        char byteArray[6] = { 0, 0, 0, 0, 0, 0 };
        try
        {
            while (SerialBT.available())
            {
                if (i < 6) {
                    byteArray[i] = SerialBT.read();
                    i += 1;
                }
                else {
                    SerialBT.read();
                }
            }
            BT_BUTTON = "";
            BT_BUTTON = BT_BUTTON + byteArray[0] + byteArray[1] + byteArray[2];
            BT_BUTTON.trim();
        }
        catch (int e) { if (isGame) return; };

        if (BT_BUTTON.equals("")) {
            if (isGame) return;
        }
        else if (BT_BUTTON.equals(BT_GAME_MOVE_LEFT)) {
            isShipMoveLeft = true;
            return;
        }
        else if (BT_BUTTON.equals(BT_GAME_MOVE_RIGHT)) {
            isShipMoveRight = true;
            return;
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM_NEXT_PATTERN)) {
            pattern = (pattern + 1) % PATTERN_NR;
            saveSpectrumPrefs();
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM_SMOOTH)) {
            if(smoothSpectrum == 0) {
                smoothSpectrum = 1;
            }
            else {
                smoothSpectrum = 0;
            }
            saveSpectrumPrefs();
            setSpectrumBands();
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM_BRIGHTNESS)) {
            getBtValue(byteArray[3], byteArray[4], byteArray[5]);
            if (bluetoothData != O) {
                brightness = bluetoothData;
                saveSpectrumPrefs();
            }
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM_GAIN)) {
            getBtValue(byteArray[3], byteArray[4], byteArray[5]);
            if (bluetoothData != O) {
                gain = bluetoothData;
                saveSpectrumPrefs();
            }
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM_SQUELCH)) {
            getBtValue(byteArray[3], byteArray[4], byteArray[5]);
            if (bluetoothData != O) {
                squelch = bluetoothData;
                saveSpectrumPrefs();
            }
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM_RESET)) {
            brightness = 50;
            gain = 30;
            squelch = 0;
            pattern = 1;
            smoothSpectrum = 0;
            saveSpectrumPrefs();
        }
        else if (BT_BUTTON.equals(BT_GET_ESP_DATA)) {
            wakeUp();
            SerialBT.println(BT_SPECTRUM_BRIGHTNESS + brightness);
            delay(10);
            SerialBT.println(BT_SPECTRUM_GAIN + gain);
            delay(10);
            SerialBT.println(BT_SPECTRUM_SQUELCH + squelch);
            delay(10);
            SerialBT.println(BT_SPECTRUM_SMOOTH + smoothSpectrum);
        }
        else if (BT_BUTTON.equals(BT_ESP_DEEP_SLEEP)) {
            deepSlepp();
        }
        else if (BT_BUTTON.equals(BT_GAME)) {
            prevIntroMillis = 0;
            isIntro = true;
            pauseAudio();
            isSpectrum = false;
            display_update_enable(true);
            initGame();
            isGame = true;
        }
        else if (BT_BUTTON.equals(BT_GAME_PLAY)) {
            isIntro = false;
            if (isEndOfGame && !isEndGameTextAnim)
                initGame();
        }
        else if (BT_BUTTON.equals(BT_SPECTRUM)) {
            isGame = false;
            display_update_enable(false);
            if (!isSpectrum) {
                resumeAudio();
            }
            isSpectrum = true;
        }
        if (isSpectrum)
            display_update_enable(true); //We can re-enable the display update
    }
}
#pragma endregion
