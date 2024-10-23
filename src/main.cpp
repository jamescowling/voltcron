// Outputs the current time as three voltages that can be used to drive a
// voltmeter clock.
//
// Hour button increments hour, minute button increments minute, second button
// resets seconds to zero. Holding second button keeps dials at max range to aid
// in adjustment.

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

// TODO: seconds reset isn't working presumably because of floatmillis
// same as range hold not working

// TODO: manually adjust zero on dials, seems slightly off and the knob isn't
// working

// Adjustment buttons.
const uint16_t HOURS_BTN_PIN = 1;
const uint16_t MINUTES_BTN_PIN = 3;
const uint16_t SECONDS_BTN_PIN = 4;
Button hourBtn(HOURS_BTN_PIN);
Button minuteBtn(MINUTES_BTN_PIN);
Button secondBtn(SECONDS_BTN_PIN);

Adafruit_MCP4728 dac;
const MCP4728_vref_t DAC_VREF = MCP4728_VREF_INTERNAL;
const MCP4728_gain_t DAC_GAIN = MCP4728_GAIN_2X;

// Output channels from DAC.
const MCP4728_channel_t HOURS_CHANNEL = MCP4728_CHANNEL_C;
const MCP4728_channel_t MINUTES_CHANNEL = MCP4728_CHANNEL_B;
const MCP4728_channel_t SECONDS_CHANNEL = MCP4728_CHANNEL_A;

// Max voltage ranges (mV) for the three voltmeter dials. Technically these
// should all be 3000 since they are 3V voltmeters but this allows some tweaking
// to fit the range into the printed scale nicely.
const uint16_t HOURS_MAX_MV = 3000;
const uint16_t MINUTES_MAX_MV = 3000;
const uint16_t SECONDS_MAX_MV = 2800;

RTC_DS3231 rtc;

const unsigned long SYNC_INTERVAL = 60000;
const unsigned long LOG_INTERVAL = 100;

// Output voltages on the three pins in mV.
uint16_t hv = 0, mv = 0, sv = 0;

// Time sync between RTC and microcontroller.
unsigned long lastSyncMillis = 0, lastRTCSeconds = 0, lastLogMillis = 0;

void adjustTime();
void synchronizeClock();
float floatSeconds();
void updateDAC(const DateTime& now);
void logTime(const DateTime& now);

void setup() {
  Serial.begin(57600);

  // Initialize RTC.
  if (!rtc.begin()) {
    while (true) {
      Serial.println("Couldn't find RTC");
      delay(1000);
    }
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize DAC.
  if (!dac.begin()) {
    while (true) {
      Serial.println("Couldn't find MCP4728 DAC");
      delay(1000);
    }
  }

  synchronizeClock();
}

void synchronizeClock() {
  Serial.println("Synchronizing clock");
  lastRTCSeconds = rtc.now().unixtime();
  lastSyncMillis = millis();
}

void loop() {
  adjustTime();

  if (millis() - lastSyncMillis >= SYNC_INTERVAL) {
    synchronizeClock();
  }

  DateTime now = rtc.now();
  updateDAC(now);

  if (millis() - lastLogMillis >= LOG_INTERVAL) {
    lastLogMillis = millis();
    logTime(now);
  }
}

void adjustTime() {
  DateTime now = rtc.now();

  if (hourBtn.IsPressed()) {
    Serial.println("hour++");
    rtc.adjust(now + TimeSpan(3600));
  }

  if (minuteBtn.IsPressed()) {
    Serial.println("minute++");
    rtc.adjust(now + TimeSpan(60));
  }

  if (secondBtn.IsPressed()) {
    Serial.println("seconds = 0");
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(),
                        now.minute(), 0));
  }
}

float floatSeconds() {
  unsigned long elapsedMillis = millis() - lastSyncMillis;
  unsigned long currentSeconds = lastRTCSeconds + elapsedMillis / 1000;
  return (currentSeconds % 60) + (elapsedMillis % 1000) / 1000.0;
}

void updateDAC(const DateTime& now) {
  // Max output while button is held, to adjust dial range.
  if (secondBtn.IsPressed()) {
    hv = HOURS_MAX_MV;
    mv = MINUTES_MAX_MV;
    sv = SECONDS_MAX_MV;
  } else {
    int h = now.hour() % 12;
    int m = now.minute();
    float s = floatSeconds();

    // Calculate voltages for hour, minute, and second hands.
    float h_seconds = h * 3600 + m * 60 + s;
    hv = (uint16_t)((h_seconds / (12 * 3600)) * HOURS_MAX_MV);
    float m_seconds = m * 60 + s;
    mv = (uint16_t)((m_seconds / 3600) * MINUTES_MAX_MV);
    sv = (uint16_t)((s / 60) * SECONDS_MAX_MV);
  }

  dac.setChannelValue(HOURS_CHANNEL, hv, DAC_VREF, DAC_GAIN);
  dac.setChannelValue(MINUTES_CHANNEL, mv, DAC_VREF, DAC_GAIN);
  dac.setChannelValue(SECONDS_CHANNEL, sv, DAC_VREF, DAC_GAIN);
}

void logTime(const DateTime& now) {
  Serial.printf("%02d:%02d:%05.2f \tH: %.2fV \tM: %.2fV \tS: %.2fV\n",
                now.hour() % 12, now.minute(), floatSeconds(), hv / 1000.0,
                mv / 1000.0, sv / 1000.0);
}
