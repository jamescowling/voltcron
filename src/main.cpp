// Outputs the current time as three voltages that can be used to drive a
// voltmeter clock.

#include <Adafruit_DotStar.h>
#include <Adafruit_MCP4728.h>
#include <Arduino.h>
#include <RTClib.h>

#include "button.h"

// Max range on the voltmeter dials, in mV.
// The MCP4728 DAC can output at most 4.096V.
// 3V dials are a good fit and available for cheap.
const uint16_t VOLTMETER_MAX = 3000;

const uint16_t HOUR_BTN_PIN = 1;
const uint16_t MINUTE_BTN_PIN = 3;
const uint16_t SECOND_BTN_PIN = 4;

RTC_DS3231 rtc;
Adafruit_MCP4728 dac;

// The Adafruit Trinket M0 comes with a built-in RGB so we'll use it.
Adafruit_DotStar strip(DOTSTAR_NUM, PIN_DOTSTAR_DATA, PIN_DOTSTAR_CLK,
                       DOTSTAR_BRG);

Button hour_btn(HOUR_BTN_PIN);
Button minute_btn(MINUTE_BTN_PIN);
Button second_btn(SECOND_BTN_PIN);

void adjustTime();
void synchronizeClocks();
float floatSeconds();
void updateDAC(const DateTime& now);
void updateLED(const DateTime& now);
void log(const DateTime& now);
void animate();

// The RTC doesn't return ms so we periodically synchronize the clocks so we can
// use the system ms offset.
unsigned long lastSyncMillis = 0;
unsigned long lastRTCSeconds = 0;
const unsigned long syncInterval = 60000;

void setup() {
  Serial.begin(57600);
  strip.begin();
  if (!rtc.begin()) {
    while (!Serial);
    Serial.println("Couldn't find DS3231 RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to build date");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!dac.begin()) {
    while (!Serial);
    Serial.println("Couldn't find MCP4728 DAC");
    while (1);
  }
  synchronizeClocks();
  animate();
}

void loop() {
  adjustTime();
  if (millis() - lastSyncMillis >= syncInterval) {
    synchronizeClocks();
  }
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

void synchronizeClocks() {
  DateTime now = rtc.now();
  lastRTCSeconds = now.unixtime();
  lastSyncMillis = millis();
}

float floatSeconds() {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - lastSyncMillis;
  unsigned long currentSeconds = lastRTCSeconds + elapsedMillis / 1000;
  float fractionalSeconds = (elapsedMillis % 1000) / 1000.0;
  return currentSeconds % 60 + fractionalSeconds;
}

void updateDAC(const DateTime& now) {
  int h = now.hour() % 12;
  int m = now.minute();
  float s = floatSeconds();

  int hv = map(h * 3600 + m * 60 + (int)s, 0, 12 * 3600, 0, VOLTMETER_MAX);
  int mv = map(m * 60 + (int)s, 0, 3600, 0, VOLTMETER_MAX);
  int sv = map((int)(s * 1000), 0, 60 * 1000, 0, VOLTMETER_MAX);

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
  if (ms - last_log < 100) {
    return;
  }
  last_log = ms;
  Serial.printf("%02d:%02d:%05.2f\n", now.hour() % 12, now.minute(),
                floatSeconds());
}

// Silly startup animation on the dials.
void animate() {
  // 3 seconds up.
  for (int i = 0; i < 300; i++) {
    Serial.println("animate");
    delay(10);
  }

  // Ramp up to VOLTMETER_MAX
  for (int value = 0; value <= VOLTMETER_MAX; value += 50) {
    dac.setChannelValue(MCP4728_CHANNEL_A, value, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    dac.setChannelValue(MCP4728_CHANNEL_B, value, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    dac.setChannelValue(MCP4728_CHANNEL_C, value, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    delay(10);  // Small delay for smooth transition
  }

  // Ramp down to 0
  for (int value = VOLTMETER_MAX; value >= 0; value -= 50) {
    dac.setChannelValue(MCP4728_CHANNEL_A, value, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    dac.setChannelValue(MCP4728_CHANNEL_B, value, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    dac.setChannelValue(MCP4728_CHANNEL_C, value, MCP4728_VREF_INTERNAL,
                        MCP4728_GAIN_2X);
    delay(10);  // Small delay for smooth transition
  }
}