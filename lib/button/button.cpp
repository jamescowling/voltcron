#include "button.h"

Button::Button(uint8_t pin, unsigned long debounce_delay)
    : pin_(pin),
      debounce_delay_(debounce_delay),
      last_button_state_(HIGH),
      last_debounce_time_(0) {
  pinMode(pin, INPUT_PULLUP);
}

// TODO review this code
bool Button::IsPressed() {
  bool reading = digitalRead(pin_);
  unsigned long current_millis = millis();

  if (reading != last_button_state_) {
    last_debounce_time_ = current_millis;
  }

  if ((current_millis - last_debounce_time_) > debounce_delay_) {
    if (reading != current_button_state_) {
      current_button_state_ = reading;
      if (current_button_state_ == LOW) {
        last_button_state_ = reading;
        return true;
      }
    }
  }

  last_button_state_ = reading;
  return false;
}