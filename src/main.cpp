// Outputs the current time as three voltages that can be used to drive a
// voltmeter clock.
//
// Hour button increments hour, minute button increments minute, second button
// resets seconds to zero. Holding second button keeps dials at max range to aid
// in adjustment.

#include <Adafruit_MCP4728.h>
#include <Arduino.h>
#include <RTClib.h>

#include "OneButton.h"

// Pin assignment:
//
//  0/SDA: I2C bus
// 1/Aout: Second reset / max range button
//  2/SCL: I2C bus
//   3/RX: Minute button
//   4/TX: Hour button
//    USB: unregulated 5V to LEDs via 47ohm resistor
//    Bat: unused
//     3V: regulated 3.3V to other boards
//    Gnd: common ground
//    Rst: unused

// Adjustment buttons.
const uint16_t HOURS_BTN_PIN = 4;
const uint16_t MINUTES_BTN_PIN = 3;
const uint16_t SECONDS_BTN_PIN = 1;
OneButton hoursBtn(HOURS_BTN_PIN);
OneButton minutesBtn(MINUTES_BTN_PIN);
OneButton secondsBtn(SECONDS_BTN_PIN);

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
const uint16_t HOURS_MAX_MV = 2860;
const uint16_t MINUTES_MAX_MV = 2860;
const uint16_t SECONDS_MAX_MV = 2805;

RTC_DS3231 rtc;

const unsigned long SYNC_INTERVAL = 60000;
const unsigned long LOG_INTERVAL = 1000;

// Time sync between RTC and microcontroller.
unsigned long lastSyncMillis = 0, lastRTCSeconds = 0, lastLogMillis = 0;

void incrHours();
void incrMinutes();
void resetSeconds();

void synchronizeClock();
float floatSeconds();
void updateDAC();

void setup() {
  Serial.begin(57600);

  // Initialize RTC.
  while (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    delay(5000);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize DAC.
  while (!dac.begin()) {
    Serial.println("Couldn't find MCP4728 DAC");
    delay(5000);
  }

  hoursBtn.attachClick(incrHours);
  minutesBtn.attachClick(incrMinutes);
  secondsBtn.attachClick(resetSeconds);

  synchronizeClock();
}

void loop() {
  hoursBtn.tick();
  minutesBtn.tick();
  secondsBtn.tick();

  updateDAC();
  delay(10);
}

void incrHours() {
  Serial.println("hour++");
  rtc.adjust(rtc.now() + TimeSpan(3600));
  synchronizeClock();
}

void incrMinutes() {
  Serial.println("minute++");
  rtc.adjust(rtc.now() + TimeSpan(60));
  synchronizeClock();
}

void resetSeconds() {
  Serial.println("seconds = 0");
  DateTime now = rtc.now();
  rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(),
                      now.minute(), 0));
  synchronizeClock();
}

void setDAC(MCP4728_channel_t channel, uint16_t mv) {
  dac.setChannelValue(channel, mv, DAC_VREF, DAC_GAIN);
}

void synchronizeClock() {
  Serial.println("Synchronizing clock");
  lastRTCSeconds = rtc.now().unixtime();
  lastSyncMillis = millis();
}

float floatSeconds() {
  unsigned long elapsedMillis = millis() - lastSyncMillis;
  unsigned long currentSeconds = lastRTCSeconds + elapsedMillis / 1000;
  return (currentSeconds % 60) + (elapsedMillis % 1000) / 1000.0;
}

void updateDAC() {
  if (millis() - lastSyncMillis >= SYNC_INTERVAL) {
    synchronizeClock();
  }

  DateTime now = rtc.now();
  int h = now.hour() % 12;
  int m = now.minute();
  int s = (int)floatSeconds();

  // Output voltages on the three pins in mV.
  uint16_t hv = 0, mv = 0, sv = 0;

  // Override output if the buttons are long-pressed,
  if (hoursBtn.isLongPressed()) {
    // Keep them at zero.
  } else if (minutesBtn.isLongPressed()) {
    hv = HOURS_MAX_MV / 2;
    mv = MINUTES_MAX_MV / 2;
    sv = SECONDS_MAX_MV / 2;
  } else if (secondsBtn.isLongPressed()) {
    hv = HOURS_MAX_MV;
    mv = MINUTES_MAX_MV;
    sv = SECONDS_MAX_MV;
  } else {
    // Calculate voltages for hour, minute, and second hands.
    // Smoothing the minute transition seems to avoid the dial getting stuck.
    // We keep the hours hand at discrete jumps since it makes it easier to read
    // and it doesn't seem to get stuck.
    hv = (h * HOURS_MAX_MV) / 12;
    mv = ((m + (s / 60.0)) * MINUTES_MAX_MV) / 60;
    sv = (s * SECONDS_MAX_MV) / 60;
  }

  setDAC(HOURS_CHANNEL, hv);
  setDAC(MINUTES_CHANNEL, mv);
  setDAC(SECONDS_CHANNEL, sv);

  if (millis() - lastLogMillis >= LOG_INTERVAL) {
    lastLogMillis = millis();
    Serial.printf("%02d:%02d:%02d \tH: %.2fV \tM: %.2fV \tS: %.2fV\n", h, m, s,
                  hv / 1000.0, mv / 1000.0, sv / 1000.0);
  }
}
