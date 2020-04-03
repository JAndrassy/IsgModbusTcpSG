
void statusLedSetup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void statusLedLopp() {
  const int BLINK_INTERVAL = 1000;
  static unsigned long previousMillis = 0;
  static boolean blinkLedState = false;

#define PWMRANGE 256

  if (millis() - previousMillis < BLINK_INTERVAL)
    return;
  previousMillis = millis();
  blinkLedState = !blinkLedState;
  if (blinkLedState) {
    if (!isgSgInput1IsON) {
      analogWrite(STATUS_LED_PIN, PWMRANGE / 2);
    } else {
      digitalWrite(STATUS_LED_PIN, HIGH);
    }
    switch (errorState) {
      case ERR_OP_STATE:
        statusLedShortBlink();
        /* no break */
      case ERR_MODBUS:
        statusLedShortBlink();
        /* no break */
      case ERR_NETWORK:
        statusLedShortBlink();
        /* no break */
      default:
        break;
    }
  } else {
    if (isgSgInput1IsON) {
      digitalWrite(STATUS_LED_PIN, LOW);
    } else {
      analogWrite(STATUS_LED_PIN, PWMRANGE / 4);
    }
  }
}

void statusLedShortBlink() {
  delay(100);
  digitalWrite(STATUS_LED_PIN, LOW);
  delay(100);
  digitalWrite(STATUS_LED_PIN, HIGH);
}
