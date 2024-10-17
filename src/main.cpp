// Outputs the current time as three voltages that can be used to drive a
// voltmeter clock.
//
// Hour button increments hour, minute button increments minute, second button
// resets seconds to zero. Holding second button keeps dials at max range to aid
// in adjustment.

#include <Adafruit_DotStar.h>
#include <Adafruit_MCP4728.h>
#include <Arduino.h>
#include <RTClib.h>

#include "button.h"

// Pin assignment:
//
//  0/SDA: I2C bus
// 1/Aout: Hour button
//  2/SCL: I2C bus
//   3/RX: Minute button
//   4/TX: Second reset / max range button
//    USB: unregulated 5V to LEDs via 47ohm resistor
//    Bat: unused
//     3V: regulated 3.3V to other boards
//    Gnd: common ground
//    Rst: unused

const uint16_t VOLTMETER_MAX = 3000;  // Max voltage for the voltmeter (mV)
const uint16_t HOUR_BTN_PIN = 1, MINUTE_BTN_PIN = 3, SECOND_BTN_PIN = 4;
const unsigned long SYNC_INTERVAL = 60000;
const unsigned long LOG_INTERVAL = 100;

RTC_DS3231 rtc;
Adafruit_MCP4728 dac;
Button hourBtn(HOUR_BTN_PIN), minuteBtn(MINUTE_BTN_PIN),
    secondBtn(SECOND_BTN_PIN);

// Time sync between RTC and microcontroller.
unsigned long lastSyncMillis = 0, lastRTCSeconds = 0, lastLogMillis = 0;

void adjustTime();
void synchronizeClocks();
float floatSeconds();
void updateDAC(const DateTime& now);
void logTime(const DateTime& now);
void animate();

void setup() {
  Serial.begin(57600);
  if (!rtc.begin()) {
    Serial.println("Couldn't find DS3231 RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!dac.begin()) {
    Serial.println("Couldn't find MCP4728 DAC");
    while (1);
  }
  synchronizeClocks();
  animate();
}

void synchronizeClocks() {
  lastRTCSeconds = rtc.now().unixtime();
  lastSyncMillis = millis();
}

void animate() {
  for (int channel = 0; channel < 3; ++channel) {
    dac.setChannelValue((MCP4728_channel_t)channel, VOLTMETER_MAX,
                        MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
    delay(1000);
  }
  for (int channel = 0; channel < 3; ++channel) {
    dac.setChannelValue((MCP4728_channel_t)channel, 0, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    delay(1000);
  }
}

void loop() {
  adjustTime();
  if (millis() - lastSyncMillis >= SYNC_INTERVAL) synchronizeClocks();
  DateTime now = rtc.now();
  updateDAC(now);
  if (millis() - lastLogMillis >= LOG_INTERVAL) {
    lastLogMillis = millis();
    logTime(now);
  }
}

void adjustTime() {
  DateTime now = rtc.now();
  if (hourBtn.IsPressed())
    rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                        (now.hour() + 1) % 24, now.minute(), now.second()));
  if (minuteBtn.IsPressed())
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(),
                        (now.minute() + 1) % 60, now.second()));
  if (secondBtn.IsPressed())
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(),
                        now.minute(), 0));
}

float floatSeconds() {
  unsigned long elapsedMillis = millis() - lastSyncMillis;
  unsigned long currentSeconds = lastRTCSeconds + elapsedMillis / 1000;
  return (currentSeconds % 60) + (elapsedMillis % 1000) / 1000.0;
}

void updateDAC(const DateTime& now) {
  int hv, mv, sv;

  // Max output while button is held so we can use it to adjust dial range.
  if (secondBtn.IsPressed()) {
    hv = mv = sv = VOLTMETER_MAX;
  } else {
    int h = now.hour() % 12;
    int m = now.minute();
    float s = floatSeconds();

    hv = map(h * 3600 + m * 60 + (int)s, 0, 12 * 3600, 0, VOLTMETER_MAX);
    mv = map(m * 60 + (int)s, 0, 3600, 0, VOLTMETER_MAX);
    sv = map((int)(s * 1000), 0, 60 * 1000, 0, VOLTMETER_MAX);
  }

  // Not sure if the delays are required but there seems to be some kind of
  // noise or inconsistency in output from the DAC so adding them just in case
  // it helps stability.
  dac.setChannelValue(MCP4728_CHANNEL_A, hv, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  delay(1);
  dac.setChannelValue(MCP4728_CHANNEL_B, mv, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  delay(1);
  dac.setChannelValue(MCP4728_CHANNEL_C, sv, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  delay(1);
}

void logTime(const DateTime& now) {
  Serial.printf("%02d:%02d:%05.2f\n", now.hour() % 12, now.minute(),
                floatSeconds());
}
