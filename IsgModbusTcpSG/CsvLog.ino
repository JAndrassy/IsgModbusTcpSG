
const char* CSV_DIR = "/CSV/";

void csvLogSetup() {
  if (sdCardAvailable && !SD.exists(CSV_DIR)) {
    SD.mkdir(CSV_DIR);
  }
}

void csvLog() {

  if (!sdCardAvailable)
    return;

  unsigned long t = now();
  char buff[32];
  sprintf(buff, "%s%d-%02d.CSV", CSV_DIR, year(t), month(t));
  File file = SD.open(buff, FILE_WRITE);
  if (file) {
    if (file.size() == 0) {
      file.println(F("timestamp;mode;i.pin;input1;op.state;wait"));
    }
    BufferedPrint bp(file, buff, sizeof(buff));
    bp.printf(F("%d-%02d-%02d %02d:%02d:%02d;%c;%d;%d;%d;%d\r\n"),
        year(t), month(t), day(t), hour(t), minute(t), second(t),
        automaticMode ? 'A' : 'M', inputPinIsON(), isgSgInput1IsON, lastKnowOpState,
        (int) ((millis() - checkSGOpStateStartMillis) / 1000));
    bp.flush();
    file.close();
  } else {
    printTimestamp();
    terminal->print(F("CSV file error "));
    terminal->println(buff);
  }
}

void csvLogPrintJson(FormattedPrint& out) {
  boolean first = true;
  out.print(F("{\"f\":["));
  if (sdCardAvailable) {
    File dir = SD.open(CSV_DIR);
    File file;
    while (file = dir.openNextFile()) { // @suppress("Assignment in condition")
      const char* fn = file.name();
      const char* ext = strchr(fn, '.');
      if (strcmp(ext, ".CSV") == 0) {
        if (first) {
          first = false;
        } else {
          out.print(',');
        }
        out.printf(F("{\"fn\":\"%s\",\"size\":%ld}"), fn, file.size());
      }
      file.close();
    }
  }
  out.print(F("]}"));
}
