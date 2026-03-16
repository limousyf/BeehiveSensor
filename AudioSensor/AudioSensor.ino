/*
 * INMP441 Microphone Test (ESP32-WROOM-32)
 *
 * Reads audio from the INMP441 I2S MEMS microphone and prints:
 *   - RMS sound level (raw and approximate dB)
 *   - Peak frequency via FFT (basic spectrum analysis)
 *
 * Wiring:
 *   INMP441 VDD  --> 3V3
 *   INMP441 GND  --> GND
 *   INMP441 L/R  --> GND (left channel)
 *   INMP441 SD   --> GPIO33 (I2S data)
 *   INMP441 SCK  --> GPIO32 (I2S bit clock)
 *   INMP441 WS   --> GPIO25 (I2S word select)
 */

#include <driver/i2s.h>
#include <math.h>

// I2S pin config
#define I2S_SD  33
#define I2S_SCK 32
#define I2S_WS  25

// I2S config
#define I2S_PORT      I2S_NUM_0
#define SAMPLE_RATE   16000
#define SAMPLE_BITS   I2S_BITS_PER_SAMPLE_32BIT
#define BLOCK_SIZE    512     // samples per read block
#define READ_INTERVAL 1000    // ms between prints

// FFT bin size = SAMPLE_RATE / BLOCK_SIZE = 31.25 Hz per bin

int32_t samples[BLOCK_SIZE];

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BLOCK_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

// Simple magnitude-only DFT for a single frequency bin
// (avoids pulling in a full FFT library for testing)
float magnitudeAtBin(int32_t *data, int n, int bin) {
  float real = 0, imag = 0;
  for (int i = 0; i < n; i++) {
    float angle = 2.0 * M_PI * bin * i / n;
    real += data[i] * cos(angle);
    imag -= data[i] * sin(angle);
  }
  return sqrt(real * real + imag * imag) / n;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n--- INMP441 Microphone Test ---");
  Serial.printf("Sample rate: %d Hz\n", SAMPLE_RATE);
  Serial.printf("Block size: %d samples\n", BLOCK_SIZE);
  Serial.printf("Bin resolution: %.1f Hz\n", (float)SAMPLE_RATE / BLOCK_SIZE);
  Serial.println("Listening...\n");

  setupI2S();
}

void loop() {
  // Read a block of samples
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
  int samplesRead = bytesRead / sizeof(int32_t);

  if (samplesRead == 0) return;

  // Debug: print first 5 raw samples to check if I2S is receiving data
  static bool firstRun = true;
  if (firstRun) {
    Serial.printf("Raw samples [0..4]: %ld %ld %ld %ld %ld\n",
      samples[0], samples[1], samples[2], samples[3], samples[4]);
    Serial.printf("Bytes read: %d, Samples: %d\n\n", bytesRead, samplesRead);
    firstRun = false;
  }

  // Shift samples — INMP441 outputs 24-bit data in the upper bits of 32-bit words
  for (int i = 0; i < samplesRead; i++) {
    samples[i] >>= 8;
  }

  // Calculate RMS
  double sumSq = 0;
  int32_t peak = 0;
  for (int i = 0; i < samplesRead; i++) {
    sumSq += (double)samples[i] * samples[i];
    if (abs(samples[i]) > peak) peak = abs(samples[i]);
  }
  double rms = sqrt(sumSq / samplesRead);
  float dbFS = (rms > 0) ? 20.0 * log10(rms / 8388607.0) : -100.0;  // 24-bit max

  // Find peak frequency — scan bins for the loudest one
  // Skip bin 0 (DC offset)
  float maxMag = 0;
  int maxBin = 1;
  int numBins = samplesRead / 2;  // Nyquist limit

  // Only scan up to 4kHz (enough for bee sounds) to save CPU
  int maxBinToScan = min(numBins, (int)(4000.0 / ((float)SAMPLE_RATE / samplesRead)));

  for (int bin = 1; bin <= maxBinToScan; bin++) {
    float mag = magnitudeAtBin(samples, samplesRead, bin);
    if (mag > maxMag) {
      maxMag = mag;
      maxBin = bin;
    }
  }
  float peakFreq = (float)maxBin * SAMPLE_RATE / samplesRead;

  // Print results
  Serial.printf("RMS: %8.0f | dBFS: %6.1f | Peak: %5.0f | PeakFreq: %6.1f Hz\n",
    rms, dbFS, (float)peak, peakFreq);

  delay(READ_INTERVAL);
}
