
const unsigned long EVENTS_SAVE_INTERVAL_SEC = 10 * 60; // sec 10 min
const char* eventLongLabels[EVENTS_SIZE] = {"Events", "Reset", "Watchdog", "Network", "Modbus", "Manual run", "OS wait"};
const unsigned short eventIsError = bit(WATCHDOG_EVENT) | bit(NETWORK_EVENT) | bit(MODBUS_EVENT) | bit(OP_STATE_TIMEOUT);

#include <EEPROM.h>
const int EVENTS_EEPROM_ADDR = 0;
const char* EVENTS_LOG_FN = "EVENTS.LOG";

struct EventStruct {
  unsigned long timestamp;
  int value1;
  int value2;
  byte count;
};

const unsigned long MIN_VALID_TIME = SECS_YR_2000 + SECS_PER_YEAR;

unsigned long eventsTimer = 0;
EventStruct events[EVENTS_SIZE];

void eventsSetup() {
  EEPROM.get(EVENTS_EEPROM_ADDR, events);
  eventsTimer = events[EVENTS_SAVE_EVENT].timestamp;
  if (eventsTimer == 0xFFFFFFFF) { // empty EEPROM
    for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
      events[i].timestamp = 0;
      events[i].count = 0;
    }
    eventsTimer = 0;
  }
  eventsWrite(RESTART_EVENT, 0, 0);
}

void eventsLoop() {
  if (now() > MIN_VALID_TIME) {
    if (events[RESTART_EVENT].timestamp < MIN_VALID_TIME) { // time was not set in setup()
      unsigned long t = now() - (millis() / 1000);
      events[RESTART_EVENT].timestamp = t;
      if (eventsTimer > t || eventsTimer < MIN_VALID_TIME) {
        eventsTimer = t;
      }
    }
    if (now() - eventsTimer > EVENTS_SAVE_INTERVAL_SEC) {
      eventsSave();
    }
  }
  if (sdCardAvailable && hour(now()) == 23 && minute(now()) == 59 && SD.exists(EVENTS_LOG_FN)) {
    SD.remove(EVENTS_LOG_FN);
  }
}

void eventsWrite(int newEvent, int value1, int value2) {
  EventStruct& e = events[newEvent];
  unsigned long last = e.timestamp;
  if (now() > MIN_VALID_TIME && last > MIN_VALID_TIME  //
      && (year(last) != year() || month(last) != month() || day(last) != day())) {
    e.count = 0; // start counting with first event of this type today
  }
  e.timestamp = now();
  e.value1 = value1;
  e.value2 = value2;
  e.count++;

  if (sdCardAvailable && newEvent != EVENTS_SAVE_EVENT) {
    File file = SD.open(EVENTS_LOG_FN, FILE_WRITE);
    if (file) {
      char buff[64];
      BufferedPrint bp(file, buff, sizeof(buff));
      eventsPrint(bp, newEvent);
      bp.println();
      bp.flush();
      file.close();
    }
  }
}

boolean eventsSaved() {
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    if (events[i].timestamp > eventsTimer)
      return false;
  }
  return true;
}

void eventsSave() {
  if (eventsSaved())
    return;
  eventsWrite(EVENTS_SAVE_EVENT, 0, 0);
  EEPROM.put(EVENTS_EEPROM_ADDR, events);
  eventsTimer = events[EVENTS_SAVE_EVENT].timestamp;
}

byte eventsRealCount(bool errorsOnly) {
  byte ec = 0;
  unsigned long midnight = previousMidnight(now());
  for (byte  i = RESTART_EVENT; i < EVENTS_SIZE; i++) {
    if (events[i].timestamp > midnight && (!errorsOnly || (eventIsError & bit(i)))) {
      ec += events[i].count;
    }
  }
  return ec;
}

void eventsPrint(Print& stream) {
  byte map[EVENTS_SIZE];
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    map[i] = i;
  }
  qsort(map, EVENTS_SIZE, 1, eventsCompare);
  stream.println(F("Event name     |timestamp          |v1   |v2   |cnt|"));
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    EventStruct& event = events[map[i]];
    if (i > 0) {
      if (event.timestamp == 0)
        continue;
    }
    eventsPrint(stream, map[i]);
    stream.println();
  }
}
void eventsPrint(Print& s, int ix) {
  unsigned long t = events[ix].timestamp;
  char buff[64];
  sprintf_P(buff, PSTR("%-15s|%d-%02d-%02d %02d:%02d:%02d|% 5d|% 5d|% 3u|"), eventLongLabels[ix],
      year(t), month(t), day(t), hour(t), minute(t), second(t),
      events[ix].value1, events[ix].value2, events[ix].count);
  s.print(buff);
}

void eventsPrintJson(FormattedPrint& stream) {
  stream.print(F("{\"e\":["));
  for (unsigned int i = 0; i < EVENTS_SIZE; i++) {
    EventStruct& event = events[i];
    if (i > 0) {
      if (event.timestamp == 0)
        continue;
      stream.print(",");
    }
    eventsPrintJson(stream, i);
  }
  stream.printf(F("],\"s\":%d}"), eventsSaved());
}

void eventsPrintJson(FormattedPrint& stream, int ix) {
  stream.printf(F("{\"i\":%i,\"t\":%lu,\"v1\":%d,\"v2\":%d,\"c\":%u}"), ix, events[ix].timestamp, events[ix].value1, events[ix].value2, events[ix].count);
}

int eventsCompare(const void * elem1, const void * elem2) {
  return (events[*((byte*) elem2)].timestamp < events[*((byte*) elem1)].timestamp) ? -1 : 1;
}
