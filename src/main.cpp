#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <MHZ.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define CO2_IN 7
#define MH_Z14_RX 2
#define MH_Z14_TX 4

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FONT_3_WIDTH 18
#define FONT_3_HEIGHT 24
#define FONT_2_WIDTH 12
#define FONT_2_HEIGHT 16

MHZ co2(MH_Z14_RX, MH_Z14_TX, CO2_IN, MHZ::MHZ14B, MHZ::RANGE_50K);
Adafruit_SSD1306 display;

uint8_t countDigits(long num) {
  uint8_t count = 0;
  while (num) {
    num = num / 10;
    count++;
  }
  return count;
}

void draw_starting_screen_progress(double completion, long delayMs) {
  int loadingBarSizeBefore = max(0, completion - 0.1) * SCREEN_WIDTH;
  int loadingBarSize = min(1, completion) * SCREEN_WIDTH;
  display.fillRect(loadingBarSizeBefore, 10, loadingBarSize - loadingBarSizeBefore, 15, SSD1306_WHITE);
  display.display();
  delay(delayMs);
}

void draw_starting_screen_initial(double maxCompletion, long totalCompletion) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(SCREEN_WIDTH / 2 - (8 * FONT_2_WIDTH) / 2, SCREEN_HEIGHT / 2 - FONT_2_HEIGHT / 2 + 15);
  display.print("Starting");
  display.drawRect(0, 10, SCREEN_WIDTH, 15, SSD1306_WHITE);

  for (size_t i = 0; i < 60; i++) draw_starting_screen_progress((double)i / 60 * maxCompletion, totalCompletion / 60);
}

void draw_indicator(int length, bool shouldDisplay) {
  if (length == 0)
    display.fillRect(0, 0, 8, 2, SSD1306_WHITE);
  else
    display.fillRect(0, 0, length, 2, SSD1306_BLACK);

  if (shouldDisplay) display.display();
}

void draw_measurement_screen(long co2ppm) {
  display.clearDisplay();
  display.setTextSize(3);

  draw_indicator(0, false);

  int totalNbDigits = countDigits(co2ppm);
  int upperPart = co2ppm / 1000;
  int upperNbDigits = countDigits(upperPart);
  int lowerPart = co2ppm % 1000;

  int partSpacing = upperPart > 0 ? 5 : 0;
  int upperPartSize = upperNbDigits * FONT_3_WIDTH;
  int lowerPartSize = (totalNbDigits - upperNbDigits) * FONT_3_WIDTH;
  int totalPartSize = upperPartSize + partSpacing + lowerPartSize;
  int margin = (SCREEN_WIDTH - totalPartSize) / 2;
  int heightOffset = SCREEN_HEIGHT - (FONT_3_HEIGHT + 2 + FONT_2_HEIGHT + 10);

  if (upperPart > 0) {
    display.setCursor(margin, heightOffset);
    display.print(upperPart);
  }
  if (lowerPart >= 0) {
    display.setCursor(SCREEN_WIDTH - margin - lowerPartSize, heightOffset);
    char buf[4];
    sprintf(buf, "%03d", lowerPart);
    display.print(buf);

    display.setTextSize(2);
    if (co2ppm == 50000) {
      display.setCursor(SCREEN_WIDTH / 2 - (5 * FONT_2_WIDTH / 2), heightOffset + FONT_3_HEIGHT + 2);
      display.print(">= ppm");
    } else {
      display.setCursor(SCREEN_WIDTH / 2 - (3 * FONT_2_WIDTH / 2), heightOffset + FONT_3_HEIGHT + 2);
      display.print("ppm");
    }
  } else {
    display.setTextSize(2);
    display.setCursor(SCREEN_WIDTH / 2 - (5 * FONT_2_WIDTH / 2), SCREEN_HEIGHT / 2 - FONT_2_HEIGHT / 2);
    display.print("Error");
  }

  display.display();
}

void serialPrintMeasurement(long co2, int temperature) {
  if (co2 < 0) {
    Serial.print("Read error: ");
    Serial.println(co2);
  } else {
    Serial.print("CO2: ");
    Serial.print(co2);
    Serial.print(" ppm ");

    Serial.print("(");
    if (temperature > 0) {
      Serial.print(temperature);
      Serial.println("°C)");
    } else {
      Serial.println("n/a");
    }
  }
}

void setup() {
  // Wire pinout: https://www.arduino.cc/reference/en/language/functions/communication/wire/
  display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);

  Serial.begin(115200);
  Serial.println("Setup start");

  // co2.setDebug(true);
  co2.setBypassCheck(true, true);
  co2.setTemperatureOffset(40);

  draw_starting_screen_initial(0.60, 20000);  // Minimal sensor warmup time

  // Make a measurement every 500ms until a valid response is received
  // Allow to terminate the loading bar as soon as the sensor is ready
  int nbTry = 0;
  do {
    int co2_ppm = co2.readCO2UART();
    // serialPrintMeasurement(co2_ppm, 0);
    if (co2_ppm > 0 && co2_ppm != 500) break;

    draw_starting_screen_progress(0.60 + nbTry * 0.04 + 0.02, 500);
    nbTry++;
    draw_starting_screen_progress(0.60 + nbTry * 0.04, 500);
  } while (nbTry < 10);
  delay(100);  // Give time to the sensor or arduino, avoid crash after setup

  // co2.calibrateZero(); // Should be done every 6 month, run for 20 min at 400ppm
  co2.setAutoCalibrate(false);  // Automatically every 24H but needs a 400ppm, recommended to be disabled for the high ppm environment
  delay(100);                   // Give it time to process the request and send the response

  Serial.println("Setup done");
}

void loop() {
  // Serial.println("loop");
  uint32_t co2_ppm = co2.readCO2UART();
  int temperature = co2.getLastTemperature();
  serialPrintMeasurement(co2_ppm, temperature);
  draw_measurement_screen(co2_ppm);

  // Pause with indicator for 4 sec
  for (size_t i = 0; i <= 7; i++) {
    delay(500);
    draw_indicator(i + 1, true);
  }
}
