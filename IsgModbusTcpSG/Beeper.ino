
void beeperSetup() {
  pinMode(BEEPER_PIN, OUTPUT);
  beep();
}

void beeperLoop() {

  const int ALARM_SOUND_INTERVAL = 3000; // 3 sec
  static unsigned long previousAlarmSoundMillis = 0;
  const int ALARM_REPEAT = 100; // 100 * 3 sec = 5 min
  static byte alarmCounter = 0;

  if (errorState != ERR_NONE && hour() > 7 && hour() < 22) {
    if (millis() - previousAlarmSoundMillis > ALARM_SOUND_INTERVAL) {
      previousAlarmSoundMillis = millis();
      if (alarmCounter < ALARM_REPEAT) {
        alarmSound();
        alarmCounter++;
      }
    }
  } else {
    alarmCounter = 0;
  }
}

void alarmSound() {
  for (int i = 0; i < 3; i++) {
    beeperTone(200);
    delay(100);
  }
}

void beep() {
  beeperTone(200);
}

// implementation for active buzzer
void beeperTone(uint32_t time) {
  digitalWrite(BEEPER_PIN, HIGH);
  delay(time);
  digitalWrite(BEEPER_PIN, LOW);
}

