
EthernetServer telnetServer(2323);
EthernetClient telnetClient;

void terminalSetup() {
  terminal = &Serial;
  telnetServer.begin();
}

void terminalLoop() {
  if (!telnetClient) {
    telnetClient = telnetServer.accept();
    if (telnetClient.connected()) {
      telnetClient.println(F("ISGweb SG Ready adapter version " VERSION));
      terminal = &telnetClient;
    } else {
      terminal = &Serial;
    }
  }

  int ch = terminal->read();
  switch (ch) {
    case 'P':
      printState();
      break;
    case 'A':
      if (automaticMode) {
        terminal->println(F("Already in automatic mode"));
      } else {
        automaticMode = true;
        terminal->println(F("Automatic mode activated"));
        switchIsgSgInput1(inputPinIsON());
      }
      break;
    case '0':
    case '1':
      if (automaticMode) {
        automaticMode = false;
        terminal->println(F("Manual mode activated"));
        eventsWrite(MANUAL_RUN_EVENT, ch - '0', 0);
      }
      switchIsgSgInput1(ch - '0');
      break;
    case 'E':
      eventsPrint(*terminal);
      break;
    case 'C':
      if (telnetClient.connected()) {
        telnetClient.stop();
      }
      break;
    case 'R': // watchdog test / reset
      eventsSave();
      terminal->println(F("Watchdog test"));
      while (terminal->read() != 'R');
      terminal->println(F("aborted"));
      break;
  }
}

void printTimestamp() {
  char buff[21];
  sprintf(buff, "%d-%02d-%02d %02d:%02d:%02d ", year(), month(), day(), hour(), minute(), second());
  terminal->print(buff);
}

