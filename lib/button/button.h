#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// A debounced button.
class Button {
 public:
  Button(uint8_t pin, unsigned long debounce_delay = 50);
  bool IsPressed();

 private:
  uint8_t pin_;
  unsigned long debounce_delay_;
  bool last_button_state_;
  bool current_button_state_;
  unsigned long last_debounce_time_;
};

#endif  // BUTTON_H
