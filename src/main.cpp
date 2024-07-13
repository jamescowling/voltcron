// Outputs the current time as three voltages that can be used to drive a
// voltmeter clock.

#include <Adafruit_DotStar.h>
#include <Adafruit_MCP4728.h>
#include <Arduino.h>
#include <RTClib.h>

#include "button.h"

const uint16_t DAC_MAX_VOLTAGE = 3000;

Adafruit_DotStar strip(DOTSTAR_NUM, PIN_DOTSTAR_DATA, PIN_DOTSTAR_CLK,
                       DOTSTAR_BRG);
RTC_DS3231 rtc;
Adafruit_MCP4728 dac;

Button hour_btn(1);
Button minute_btn(3);
Button second_btn(4);

void adjustTime();
void updateDAC(const DateTime& now);
void updateLED(const DateTime& now);
void log(const DateTime& now);

void setup() {
  strip.begin();

  // Init RTC and DAC.
  Serial.begin(57600);
  if (!rtc.begin()) {
    Serial.println("Couldn't find DS3231 RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to build date");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!dac.begin()) {
    Serial.println("Couldn't find MCP4728 DAC");
    while (1);
  }
}

void loop() {
  adjustTime();
  DateTime now = rtc.now();
  updateDAC(now);
  updateLED(now);
  log(now);
}

void adjustTime() {
  if (hour_btn.IsPressed()) {
    Serial.println("hour++");
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                        (now.hour() + 1) % 24, now.minute(), now.second()));
  }
  if (minute_btn.IsPressed()) {
    Serial.println("minute++");
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(),
                        (now.minute() + 1) % 60, now.second()));
  }
  if (second_btn.IsPressed()) {
    Serial.println("second=0");
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(),
                        now.minute(), 0));
  }
}

void updateDAC(const DateTime& now) {
  int h = now.hour() % 12;
  int m = now.minute();
  int s = now.second();

  int hv = map(h * 3600 + m * 60 + s, 0, 12 * 3600, 0, DAC_MAX_VOLTAGE);
  int mv = map(m * 60 + s, 0, 3600, 0, DAC_MAX_VOLTAGE);
  int sv = map(s, 0, 60, 0, DAC_MAX_VOLTAGE);

  dac.setChannelValue(MCP4728_CHANNEL_A, hv, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_B, mv, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_C, sv, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  dac.saveToEEPROM();
}

void updateLED(const DateTime& now) {
  int h = now.hour() % 12;
  int m = now.minute();
  int s = now.second();

  int hc = map(h, 0, 11, 0, 255);
  int mc = map(m, 0, 60, 0, 255);
  int sc = map(s, 0, 60, 0, 255);

  strip.setPixelColor(0, hc, mc, sc);
  strip.show();
}

unsigned long last_log = 0;
void log(const DateTime& now) {
  unsigned long ms = millis();
  if (ms - last_log < 1000) {
    return;
  }
  last_log = ms;
  Serial.printf("%02d:%02d:%02d\n", now.hour() % 12, now.minute(),
                now.second());
}