
const unsigned long TIME_SYNC_INTERVAL = SECS_PER_DAY; // seconds
unsigned long previousTimeSync;

void symoRtcSetup() {
  symoRtcQuery();
}

void symoRtcLoop() {
  static unsigned long symoRtcRetryMillis = 0;

  if ((now() - previousTimeSync > TIME_SYNC_INTERVAL || previousTimeSync == 0) && millis() - symoRtcRetryMillis > 5000) {
    symoRtcRetryMillis = millis();
    bool ok = symoRtcQuery();
    if (!ok && previousTimeSync > 0) { // even in case of error, if time was retrieved at least once
      previousTimeSync = now(); // try again only next day
    }
  }
}

bool symoRtcQuery() {

  printTimestamp();
  terminal->println(F("Request time from Symo."));

  EthernetClient client;

  if (!client.connect(symoAddress, 502)) {
    printTimestamp();
    terminal->println(F("Error: connection to Symo failed"));
    eventsWrite(NETWORK_EVENT, 1, 0);
    return false;
  }
  short regs[2];
  int res = modbusRequest(client, FNC_H_READ_REGS, 40222, 2, regs);
  client.stop();
  if (res != 0) {
    printTimestamp();
    terminal->print(F("Error reading time, error code: "));
    terminal->println(res);
    eventsWrite(MODBUS_EVENT, 5, res);
    return false;
  }
  // SunSpec has seconds from 1.1.2000 UTC, TimeLib uses 'epoch' (seconds from 1.1.1970)
  setTime(SECS_YR_2000 + (unsigned short) regs[0] * 65536L + (unsigned short) regs[1] + SECS_PER_HOUR);
  previousTimeSync = now();
  printTimestamp();
  terminal->println(F("Time retrieved."));
  return true;
}

