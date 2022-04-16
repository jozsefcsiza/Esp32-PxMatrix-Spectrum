/*
 * This code has been taken from the sound reactive fork of WLED by Andrew
 * Tuline, and the analog audio processing has been (mostly) removed as we
 * will only be using the INMP441.
 *
 * The FFT code runs on core 0 while everything else runs on core 1. This
 * means we can make our main code more complex without affecting the FFT
 * processing.
 */

#include <driver/i2s.h>
#include <arduinoFFT.h>

#define I2S_WS 4               // aka LRCL -> IO4
#define I2S_SD 32              // aka DOUT -> IO32
#define I2S_SCK 12             // aka BCLK -> IO12
#define MIN_SHOW_DELAY  15
#define MIN_FREQUENCY 120      //Hz -> Don't use lower than 60Hz
#define MAX_FREQUENCY 1050     //Hz -> Don't use higher than 5120Hz

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int BLOCK_SIZE = 64;

const int SAMPLE_RATE = 10240;

TaskHandle_t FFT_Task;

int squelch = 0;                           // Squelch, cuts out low level sounds
int gain = 30;                             // Gain, boosts input level*/
uint16_t micData;                          // Analog input for FFT
uint16_t micDataSm;                        // Smoothed mic data, as it's a bit twitchy
uint16_t smoothSpectrum = 0;
uint16_t NUM_BANDS = 16;

const uint16_t samples = 1024;              // This value MUST ALWAYS be a power of 2
unsigned int sampling_period_us;
unsigned long microseconds;

double FFT_MajorPeak = 0;
double FFT_Magnitude = 0;
uint16_t mAvg = 0;

// These are the input and output vectors.  Input vectors receive computed results from FFT.
double vReal[samples];
double vImag[samples];
double fftBin[samples];

// Try and normalize fftBin values to a max of 4096, so that 4096/16 = 256.
// Oh, and bins 0,1,2 are no good, so we'll zero them out.
double fftCalc[M_WIDTH];
int fftResult[M_WIDTH];                      // Our calculated result table, which we feed to the animations.
double fftResultMax[M_WIDTH];                // A table used for testing to determine how our post-processing is working.

// Table of linearNoise results to be multiplied by squelch in order to reduce squelch across fftResult bins.
int linearNoise[M_WIDTH] = { 34, 28, 26, 25, 20, 12, 9, 6, 4, 4, 3, 2, 2, 2, 2, 2 };

// Table of multiplication factors so that we can even out the frequency response.
double fftResultPink[M_WIDTH] = { 1.70,1.71,1.73,1.78,1.68,1.56,1.55,1.63,1.79,1.62,1.80,2.06,2.47,3.35,6.83,9.55 };

int lowBin[M_WIDTH + 1];
int highBin[M_WIDTH + 1];
int centerBin[M_WIDTH + 1];
int divBin[M_WIDTH + 1];

// Create FFT object
arduinoFFT FFT = arduinoFFT(vReal, vImag, samples, SAMPLE_RATE);

double fftAdd(int from, int to) {
    int i = from;
    double result = 0;
    while (i <= to) {
        result += fftBin[i++];
    }
    return result;
}

void fftBinCalc(double lowFreq, double highFreq) {
    int niqFreq = SAMPLE_RATE / 2;
    double freqMultipl = pow(highFreq / lowFreq, 1.0 / (double)(NUM_BANDS - 1));
    double binWidth = (double)(SAMPLE_RATE / samples);
    double freq;

    for (int i = 0; i < NUM_BANDS + 1; i++) {
        freq = lowFreq * pow(freqMultipl, i);
        centerBin[i] = freq / binWidth;
    }
    for (int i = 0; i < NUM_BANDS; i++) {
        highBin[i] = ((centerBin[i + 1] - centerBin[i]) / 2.0) + centerBin[i];
        if (highBin[i] >= samples / 2)
            highBin[i] == samples / 2 - 1;
    }
    for (int i = 0; i < NUM_BANDS; i++) {
        if (i == 0) {
            lowBin[i] = highBin[i] - 1;
        }
        else {
            lowBin[i] = highBin[i - 1];
        }
        if (lowBin[i] >= samples / 2)
            lowBin[i] == samples / 2 - 1;
        divBin[i] = highBin[i] - lowBin[i] + 1;
    }
}

// FFT main code
void FFTcode(void* parameter) {

    for (;;) {
        delay(1);           // DO NOT DELETE THIS LINE! It is needed to give the IDLE(0) task enough time and to keep the watchdog happy.
                            // taskYIELD(), yield(), vTaskDelay() and esp_task_wdt_feed() didn't seem to work.

        microseconds = micros();

        for (int i = 0; i < samples; i++) {
            int32_t digitalSample = 0;
            int bytes_read = i2s_pop_sample(I2S_PORT, (char*)&digitalSample, portMAX_DELAY); // no timeout
            if (bytes_read > 0) {
                micData = abs(digitalSample >> 16);
            }

            //micDataSm = ((micData * 3) + micData)/4;  // We'll be passing smoothed micData to the volume routines as the A/D is a bit twitchy (not used here)
            vReal[i] = micData;                       // Store Mic Data in an array
            vImag[i] = 0;
            microseconds += sampling_period_us;
        }

        FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);      // Weigh data
        FFT.Compute(FFT_FORWARD);                             // Compute FFT
        FFT.ComplexToMagnitude();                             // Compute magnitudes

        //
        // vReal[3 .. 255] contain useful data, each a 20Hz interval (60Hz - 5120Hz).
        // There could be interesting data at bins 0 to 2, but there are too many artifacts.
        //
        FFT.MajorPeak(&FFT_MajorPeak, &FFT_Magnitude);          // let the effects know which freq was most dominant

        for (int i = 0; i < samples; i++) {                     // Values for bins 0 and 1 are WAY too large. Might as well start at 3.
            double t = 0.0;
            t = abs(vReal[i]);
            t = t / 16.0;                                       // Reduce magnitude. Want end result to be linear and ~4096 max.
            fftBin[i] = t;
        } // for()


        /* This FFT post processing is a DIY endeavour. What we really need is someone with sound engineering expertise to do a great job here AND most importantly, that the animations look GREAT as a result.
         *
         * Andrew's updated mapping of 256 bins down to the 16 result bins with Sample Freq = 10240, samples = 512 and some overlap.
         * Based on testing, the lowest/Start frequency is 60 Hz (with bin 3) and a highest/End frequency of 5120 Hz in bin 255.
         * Now, Take the 60Hz and multiply by 1.320367784 to get the next frequency and so on until the end. Then detetermine the bins.
         * End frequency = Start frequency * multiplier ^ 16
         * Multiplier = (End frequency/ Start frequency) ^ 1/16
         * Multiplier = 1.320367784
         */

        for (int i = 0; i < NUM_BANDS; i++) {
            fftCalc[i] = (fftAdd(lowBin[i], highBin[i])) / divBin[i];
        }

        // Noise supression of fftCalc bins using squelch adjustment for different input types.
        for (int i = 0; i < NUM_BANDS; i++) {
            fftCalc[i] = fftCalc[i] - (float)squelch * (float)linearNoise[i] / 4.0 <= 0 ? 0 : fftCalc[i];
        }

        // Adjustment for frequency curves.
        for (int i = 0; i < NUM_BANDS; i++) {
            fftCalc[i] = fftCalc[i] * fftResultPink[i];
        }

        // Manual linear adjustment of gain using gain adjustment for different input types.
        for (int i = 0; i < NUM_BANDS; i++) {
            fftCalc[i] = fftCalc[i] * gain / 40 + fftCalc[i] / 16.0;
        }

        // Now, let's dump it all into fftResult. Need to do this, otherwise other routines might grab fftResult values prematurely.
        for (int i = 0; i < NUM_BANDS; i++) {
            fftResult[i] = constrain((int)fftCalc[i], 0, 254);
        }

    } // for(;;)
} // FFTcode()

void setupAudio() {

    // Attempt to configure INMP441 Microphone
    esp_err_t err;
    const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),  // Receive, not transfer
      .sample_rate = SAMPLE_RATE * 2,                     // 10240 * 2 (20480) Hz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,       // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,        // LEFT when pin is tied to ground.
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,           // Interrupt level 1
      .dma_buf_count = 8,                                 // number of buffers
      .dma_buf_len = BLOCK_SIZE                           // samples per buffer
    };
    const i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,      // BCLK aka SCK
      .ws_io_num = I2S_WS,        // LRCL aka WS
      .data_out_num = -1,         // not used (only for speakers)
      .data_in_num = I2S_SD       // DOUT aka SD
    };

    // Configuring the I2S driver and pins.
    // This function must be called before any I2S driver read/write operations.
    err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed installing driver: %d\n", err);
        while (true);
    }
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed setting pin: %d\n", err);
        while (true);
    }
    Serial.println("I2S driver installed.");
    delay(100);


    // Test to see if we have a digital microphone installed or not.
    float mean = 0.0;
    int32_t samples[BLOCK_SIZE];
    int num_bytes_read = i2s_read_bytes(I2S_PORT,
        (char*)samples,
        BLOCK_SIZE,     // the doc says bytes, but its elements.
        portMAX_DELAY); // no timeout

    int samples_read = num_bytes_read / 8;
    if (samples_read > 0) {
        for (int i = 0; i < samples_read; ++i) {
            mean += samples[i];
        }
        mean = mean / BLOCK_SIZE / 16384;
        if (mean != 0.0) {
            Serial.println("Digital microphone is present.");
        }
        else {
            Serial.println("Digital microphone is NOT present.");
        }
    }

    sampling_period_us = round(1000000 * (1.0 / SAMPLE_RATE));

    // Define the FFT Task and lock it to core 0
    xTaskCreatePinnedToCore(
        FFTcode,                          // Function to implement the task
        "FFT",                            // Name of the task
        10000,                            // Stack size in words
        NULL,                             // Task input parameter
        1,                                // Priority of the task
        &FFT_Task,                        // Task handle
        0);                               // Core where the task should run
} //setupAudio()

void pauseAudio() {
    if (FFT_Task != NULL) {
        vTaskSuspend(FFT_Task);
    }
}

void resumeAudio() {
    if (FFT_Task != NULL) {
        vTaskResume(FFT_Task);
    }
}

void setSpectrumBands() {
    NUM_BANDS = M_WIDTH / 4;
    if (smoothSpectrum == 1) {
        NUM_BANDS = M_WIDTH;
    }

    //We need at least 16 bands!!!
    if (NUM_BANDS < 16)
        NUM_BANDS == 16;
    int step = NUM_BANDS / 16;
    if (step < 1)
        step = 1;

    for (int i = 0; i < NUM_BANDS; i++) {
        if (i >= 0 && i < (step * 1)) {
            linearNoise[i] = 34;
            fftResultPink[i] = 1.70;
        }
        else if (i >= (step * 1) && i < (step * 2)) {
            linearNoise[i] = 28;
            fftResultPink[i] = 1.71;
        }
        else if (i >= (step * 2) && i < (step * 3)) {
            linearNoise[i] = 26;
            fftResultPink[i] = 1.73;
        }
        else if (i >= (step * 3) && i < (step * 4)) {
            linearNoise[i] = 25;
            fftResultPink[i] = 1.78;
        }
        else if (i >= (step * 4) && i < (step * 5)) {
            linearNoise[i] = 20;
            fftResultPink[i] = 1.68;
        }
        else if (i >= (step * 5) && i < (step * 6)) {
            linearNoise[i] = 12;
            fftResultPink[i] = 1.56;
        }
        else if (i >= (step * 6) && i < (step * 7)) {
            linearNoise[i] = 9;
            fftResultPink[i] = 1.55;
        }
        else if (i >= (step * 7) && i < (step * 8)) {
            linearNoise[i] = 6;
            fftResultPink[i] = 1.63;
        }
        else if (i >= (step * 8) && i < (step * 9)) {
            linearNoise[i] = 4;
            fftResultPink[i] = 1.79;
        }
        else if (i >= (step * 9) && i < (step * 10)) {
            linearNoise[i] = 4;
            fftResultPink[i] = 1.62;
        }
        else if (i >= (step * 10) && i < (step * 11)) {
            linearNoise[i] = 3;
            fftResultPink[i] = 1.80;
        }
        else if (i >= (step * 11) && i < (step * 12)) {
            linearNoise[i] = 2;
            fftResultPink[i] = 2.06;
        }
        else if (i >= (step * 12) && i < (step * 13)) {
            linearNoise[i] = 2;
            fftResultPink[i] = 2.47;
        }
        else if (i >= (step * 13) && i < (step * 14)) {
            linearNoise[i] = 2;
            fftResultPink[i] = 3.35;
        }
        else if (i >= (step * 14) && i < (step * 15)) {
            linearNoise[i] = 2;
            fftResultPink[i] = 6.83;
        }
        else {
            linearNoise[i] = 2;
            fftResultPink[i] = 9.55;
        }
    }
    fftBinCalc(MIN_FREQUENCY, MAX_FREQUENCY);
}