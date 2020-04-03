
enum struct RestRequest {
  INDEX = 'I',
  EVENTS = 'E',
  CSV_LIST = 'L',
  AUTOMATIC_MODE = 'A',
  MANUAL_0 = '0',
  MANUAL_1 = '1',
  QUERY_OP_STATE = 'Q'
};

EthernetServer webServer(80);

void webServerSetup() {
  webServer.begin();
}

void webServerLoop() {

  EthernetClient client = webServer.available();
  if (!client)
    return;
  if (client.connected()) {
    if (client.find(' ')) { // GET /fn HTTP/1.1
      char fn[20];
      int l = client.readBytesUntil(' ', fn, sizeof(fn));
      fn[l] = 0;
      while (client.read() != -1);
      if (l == 1) {
        strcpy(fn, "/index.html");
      }
      printTimestamp();
      terminal->print(F("web server: "));
      terminal->println(fn);
      char buff[512];
      ChunkedPrint chunked(client, buff, sizeof(buff));
      if (l == 2 && strchr("IELA01Q", fn[1])) { // valid rest requests
        webServerRestRequest(fn[1], chunked);
      } else {
        webServerServeFile(fn, chunked);
      }
    }
    client.stop();
  }
}

void webServerRestRequest(char cmd, ChunkedPrint& chunked) {
  RestRequest request = (RestRequest) cmd;
  chunked.println(F("HTTP/1.1 200 OK"));
  chunked.println(F("Connection: close"));
  chunked.println(F("Content-Type: application/json"));
  chunked.println(F("Transfer-Encoding: chunked"));
  chunked.println(F("Cache-Control: no-store"));
  chunked.println(F("Access-Control-Allow-Origin: *"));
  chunked.println();
  chunked.begin();
  switch (request) {
    default:
      printValuesJson(chunked);
      break;
    case RestRequest::EVENTS:
      eventsPrintJson(chunked);
      break;
    case RestRequest::CSV_LIST:
      csvLogPrintJson(chunked);
      break;
    case RestRequest::AUTOMATIC_MODE:
      if (!automaticMode) {
        automaticMode = true;
        switchIsgSgInput1(inputPinIsON());
      }
      printValuesJson(chunked);
      break;
    case RestRequest::MANUAL_0:
    case RestRequest::MANUAL_1:
      if (automaticMode) {
        automaticMode = false;
        eventsWrite(MANUAL_RUN_EVENT, (char) request - '0', 0);
      }
      switchIsgSgInput1((char) request - '0');
      printValuesJson(chunked);
      break;
    case RestRequest::QUERY_OP_STATE:
      queryState();
      printValuesJson(chunked);
      break;
  }
  chunked.end();
}

void webServerServeFile(const char *fn, BufferedPrint& bp) {
  boolean notFound = true;
  char* ext = strchr(fn, '.');
  if (sdCardAvailable) {
    if (strlen(ext) > 4) {
      ext[4] = 0;
      memmove(ext + 2, ext, 5);
      ext[0] = '~';
      ext[1] = '1';
      ext += 2;
    }
    File dataFile = SD.open(fn);
    if (dataFile) {
      notFound = false;
      bp.println(F("HTTP/1.1 200 OK"));
      bp.println(F("Connection: close"));
      bp.print(F("Content-Length: "));
      bp.println(dataFile.size());
      bp.print(F("Content-Type: "));
      bp.println(getContentType(ext));
      if (strcmp(ext, ".CSV") == 0) {
        bp.println(F("Content-Disposition: attachment"));
      } else if (strcmp(ext, ".LOG") == 0) {
        bp.println(F("Cache-Control: no-store"));
      } else {
        unsigned long expires = now() + SECS_PER_YEAR;
        bp.printf(F("Expires: %s, "), dayShortStr(weekday(expires))); // two printfs because ShortStr functions share the buffer
        bp.printf(F("%d %s %d 00:00:00 GMT"), day(expires), monthShortStr(month(expires)), year(expires));
        bp.println();
      }
      bp.println();
      uint16_t c = 0;
      while (dataFile.available()) {
        bp.write(dataFile.read());
        if ((c++) == 50000) {
          wdt_reset();
          c = 0;
        }
      }
      dataFile.close();
      bp.flush();
    }
  }
  if (notFound) {
    bp.println(F("HTTP/1.1 404 Not Found"));
    bp.printf(F("Content-Length: "));
    bp.println(12 + strlen(fn));
    bp.println();
    bp.printf(F("\"%s\" not found"), fn);
    bp.flush();
  }
}

void printValuesJson(FormattedPrint& client) {
  client.printf(F("{\"v\":\"v" VERSION "\",\"mode\":\"%c\",\"i\":\"%s\",\"o\":%d,\"op\":%d,\"ec\":%d,\"csv\":\"list\""),
      automaticMode ? 'A' : 'M', inputPinIsON() ? "ON" : "OFF", isgSgInput1IsON, lastKnowOpState, eventsRealCount(false));
  if (errorState != ERR_NONE) {
    client.printf(F(",\"es\":\"%c\""), errorState);
  }
  if (checkSGOpStateStartMillis) {
    client.printf(F(",\"w\":%d"), (int)((millis() - checkSGOpStateStartMillis) / 1000));
  }
  byte errCount = eventsRealCount(true);
  if (errCount) {
    client.printf(F(",\"err\":%d"), errCount);
  }
  client.print('}');
}

const char* getContentType(const char* ext){
  if (!strcmp(ext, ".html") || !strcmp(ext, ".htm"))
    return "text/html";
  if (!strcmp(ext, ".css"))
    return "text/css";
  if (!strcmp(ext, ".js"))
    return "application/javascript";
  if (!strcmp(ext, ".png"))
    return "image/png";
  if (!strcmp(ext, ".gif"))
    return "image/gif";
  if (!strcmp(ext, ".jpg"))
    return "image/jpeg";
  if (!strcmp(ext, ".ico"))
    return "image/x-icon";
  if (!strcmp(ext, ".xml"))
    return "text/xml";
  return "text/plain";
}
