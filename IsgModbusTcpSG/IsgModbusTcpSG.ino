
#include <avr/wdt.h>
#include <UIPEthernet.h> // or <Ethernet.h>
#include <SD.h>
#include <TimeLib.h>
#include <StreamLib.h>

#define VERSION "1.00"

const byte FNC_H_READ_REGS = 0x03;
const byte FNC_I_READ_REGS = 0x04;
const byte FNC_WRITE_SINGLE = 0x06;
const byte FNC_ERR_FLAG = 0x80;
const int MODBUS_CONNECT_ERROR = -10;
const int MODBUS_NO_RESPONSE = -11;

const byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const byte INPUT_PIN = 3;
const byte SD_CS_PIN = 4;
const byte BEEPER_PIN = 8;
const byte STATUS_LED_PIN = 9; // PWM pin
const byte ETH_CS_PIN = 10;

const unsigned short CHECK_OP_STATE_INTERVAL = 15; // seconds
const unsigned short CHECK_OP_STATE_TIMEOUT = 5 * 60; // 5 minutes in seconds

const IPAddress ip(192, 168, 1, 110);
const IPAddress symoAddress(192, 168, 1, 7);
const IPAddress isgAddress(192, 168, 1, 108);

Stream* terminal;
bool sdCardAvailable;

enum {
  ERR_NONE = ' ',
  ERR_NETWORK = 'N',
  ERR_MODBUS = 'M',
  ERR_OP_STATE = 'O'
};

char errorState = ERR_NONE;
bool automaticMode = true;
bool isgSgInput1IsON;
byte lastKnowOpState;
unsigned long checkSGOpStateStartMillis;
unsigned long checkSGOpStateCheckMillis;

// events array indexes (and size)
enum {
  EVENTS_SAVE_EVENT,
  RESTART_EVENT,
  WATCHDOG_EVENT,
  NETWORK_EVENT,
  MODBUS_EVENT,
  MANUAL_RUN_EVENT,
  OP_STATE_TIMEOUT,
  EVENTS_SIZE
};

void setup() {

  beeperSetup();

  Serial.begin(115200);
  terminal = &Serial;

  pinMode(INPUT_PIN, INPUT_PULLUP);

  Serial.println(F("ISGweb SG Ready adapter version " VERSION));

  if (SD.begin(SD_CS_PIN)) {
    sdCardAvailable = true;
    Serial.println(F("SD card initialized"));
  }

  Ethernet.init(ETH_CS_PIN);
  Ethernet.begin(mac, ip);

  terminalSetup();
  symoRtcSetup();
  eventsSetup();
  watchdogSetup();
  csvLogSetup();
  webServerSetup();
  statusLedSetup();

  beep();
  queryState();
}

void loop() {

  wdt_reset(); // feed the dog
  Ethernet.maintain();
  eventsLoop();
  statusLedLopp();
  beeperLoop();
  terminalLoop();
  webServerLoop();

  if (handleErrorState())
    return;
  if (!networkConnected())
    return;
  symoRtcLoop();

  if (automaticMode) {
    bool on = inputPinIsON();
    if (isgSgInput1IsON != on) {
      printTimestamp();
      terminal->print(F("Signal changed to "));
      terminal->println(on ? "ON" : "OFF");
      switchIsgSgInput1(on);
    }
  }

  if (checkSGOpStateStartMillis && millis() - checkSGOpStateCheckMillis > (1000UL * CHECK_OP_STATE_INTERVAL)) {
    checkSGOpState();
    checkSGOpStateCheckMillis = millis();
    if (checkSGOpStateStartMillis && millis() - checkSGOpStateStartMillis > (1000L * CHECK_OP_STATE_TIMEOUT)) {
      csvLog();
      printTimestamp();
      terminal->println(F("Waiting for op.state change timed-out."));
      checkSGOpStateStartMillis = 0; // stop checking
      eventsWrite(OP_STATE_TIMEOUT, isgSgInput1IsON, lastKnowOpState);
      errorState = ERR_OP_STATE;
    }
  }
}

bool inputPinIsON() {
  return (digitalRead(INPUT_PIN) == LOW);
}

void switchIsgSgInput1(bool on) {

  printTimestamp();
  terminal->print(F("Setting SG INPUT1 register to "));
  terminal->println(on ? "ON" : "OFF");

  EthernetClient client;
  if (!client.connect(isgAddress, 502)) {
    client.stop();
    printTimestamp();
    terminal->println(F("Error: connection failed"));
    modbusError(MODBUS_CONNECT_ERROR);
    return;
  }
  int res = modbusWriteSingle(client, 4001, on);
  client.stop();
  if (modbusError(res)) {
    printTimestamp();
    terminal->print(F("Error setting SG INPUT1 register. error code: "));
    terminal->println(res);
    delay(1000);
    return;
  }
  isgSgInput1IsON = on;
  checkSGOpStateStartMillis = millis();
  csvLog();
}

bool checkSGOpState() {
  EthernetClient client;
  if (!client.connect(isgAddress, 502)) {
    client.stop();
    printTimestamp();
    terminal->println(F("Error: connection failed"));
    modbusError(MODBUS_CONNECT_ERROR);
    return false;
  }
  short regs[1];
  int res = modbusRequest(client, FNC_I_READ_REGS, 5000, 1, regs);
  client.stop();
  printTimestamp();
  if (modbusError(res)) {
    terminal->print(F("Error reading register 5001, error code: "));
    terminal->println(res);
    return false;
  }
  terminal->print(F("Register 5001 (SG READY OPERATING STATE): "));
  terminal->println(regs[0]);
  lastKnowOpState = regs[0];
  if (regs[0] == (isgSgInput1IsON ? 3 : 2)) { // operating states 2 and 3
    printTimestamp();
    terminal->print(F("Operating state changed to "));
    terminal->println(regs[0]);
    csvLog();
    checkSGOpStateStartMillis = 0;
    return true;
  }
  return false;
}

void printState() {

  terminal->println();
  if (errorState != ERR_NONE) {
    terminal->print("ERROR STATE! cause: ");
    switch (errorState) {
      case ERR_NETWORK:
        terminal->println("network");
        break;
      case ERR_MODBUS:
        terminal->println("modbus");
        break;
      case ERR_OP_STATE:
        terminal->println("op.state");
        break;
    }
  }
  terminal->println(automaticMode ? F("Automatic mode") : F("Manual mode"));

  terminal->print(F("Last SG INPUT1 command was "));
  terminal->println(isgSgInput1IsON ? "ON" : "OFF");

  bool inputPinIsON = (digitalRead(INPUT_PIN) == LOW);
  terminal->print(F("Input pin state is "));
  terminal->println(inputPinIsON ? "ON" : "OFF");

  queryState();
  terminal->println();
}


bool queryState() {
  EthernetClient client;
  if (!client.connect(isgAddress, 502)) {
    client.stop();
    terminal->println(F("Error: connection failed"));
    modbusError(MODBUS_CONNECT_ERROR);
    return false;
  }

  short regs[3] = {0, 0, 0};

  int res = modbusRequest(client, FNC_H_READ_REGS, 4000, 3, regs);
  if (modbusError(res)) {
    client.stop();
    terminal->print(F("modbus error "));
    terminal->println(res);
    return false;
  }
//  terminal->print(F("Register 4001 (SG READY ON/OFF): "));
//  terminal->println(regs[0]);
  terminal->print(F("Register 4002 (SG READY INPUT 1): "));
  terminal->println(regs[1]);
  isgSgInput1IsON = regs[1];
//  terminal->print(F("Register 4003 (SG READY INPUT 2): "));
//  terminal->println(regs[2]);

  res = modbusRequest(client, FNC_I_READ_REGS, 5000, 1, regs);
  client.stop();
  if (modbusError(res)) {
    terminal->print(F("modbus error "));
    terminal->println(res);
    return false;
  }
  terminal->print(F("Register 5001 (SG READY OPERATING STATE): "));
  terminal->println(regs[0]);
  lastKnowOpState = regs[0];
  return true;
}

bool handleErrorState() {

  static unsigned long modbusCheckMillis;
  const unsigned long MODBUS_CHECK_INTERVAL = 5000;
  const unsigned long OP_STATE_CHECK_INTERVAL = 60000;

  if (errorState == ERR_NONE)
    return false;;
  boolean stopAlarm = false;
  switch (errorState) {
    case ERR_NETWORK:
      stopAlarm = (Ethernet.linkStatus() != LinkOFF);
      break;
    case ERR_MODBUS:
      if (millis() - modbusCheckMillis > MODBUS_CHECK_INTERVAL) {
        stopAlarm = queryState();
        modbusCheckMillis = millis();
      }
      break;
    case ERR_OP_STATE:
      if (millis() - modbusCheckMillis > OP_STATE_CHECK_INTERVAL) {
        stopAlarm = checkSGOpState();
        modbusCheckMillis = millis();
      }
      break;
    default:
      break;
  }
  if (stopAlarm) {
    errorState = ERR_NONE;
    printTimestamp();
    terminal->println(F("Error state resolved."));
    return false;
  }
  return true;
}

boolean networkConnected() {
  static int tryCount = 0;
  if (Ethernet.linkStatus() != LinkOFF) {
    tryCount = 0;
    return true;
  }
  tryCount++;
  if (tryCount == 30) {
    eventsWrite(NETWORK_EVENT, -1, 0);
    errorState = ERR_NETWORK;
    printTimestamp();
    terminal->println(F("Network error state."));
  }
  return false;
}

void watchdogSetup() {
#ifdef ARDUINO_ARCH_MEGAAVR
  if (RSTCTRL.RSTFR & RSTCTRL_WDRF_bm) {
    printTimestamp();
    terminal->println(F("It was a watchdog reset."));
    eventsWrite(WATCHDOG_EVENT, 0, 0);
  }
  RSTCTRL.RSTFR |= RSTCTRL_WDRF_bm ;
  wdt_enable(WDT_PERIOD_8KCLK_gc);
#endif
}

boolean modbusError(int err) {

  static byte errorCounter = 0;
  static int errorCode = 0;

  if (errorCode != err) {
    errorCounter = 0;
    errorCode = err;
  }
  if (err == 0)
    return false;
  errorCounter++;
  if (errorCounter == 5 && errorState == ERR_NONE) {
    eventsWrite(MODBUS_EVENT, err, 0);
    errorState = ERR_MODBUS;
    printTimestamp();
    terminal->println(F("MODBUS error state."));
  }
  return true;
}

/*
 * return
 *   - 0 is success
 *   - negative is comm error
 *   - positive value is client protocol exception code
 */
int modbusRequest(Client& client, byte fnc, unsigned int addr, byte len, short *regs) {

  const byte CODE_IX = 7;
  const byte ERR_CODE_IX = 8;
  const byte LENGTH_IX = 8;
  const byte DATA_IX = 9;

  client.setTimeout(3000);

  byte request[] = {0, 1, 0, 0, 0, 6, 1, fnc, (byte) (addr / 256), (byte) (addr % 256), 0, len};
  client.write(request, sizeof(request));

  int respDataLen = len * 2;
  byte response[max((int) DATA_IX, respDataLen)];
  int readLen = client.readBytes(response, DATA_IX);
  if (readLen < DATA_IX)
    return MODBUS_NO_RESPONSE;
  if (response[CODE_IX] == (FNC_ERR_FLAG | fnc))
    return response[ERR_CODE_IX]; // 0x01, 0x02, 0x03 or 0x11
  if (response[CODE_IX] != fnc)
    return -2;
  if (response[LENGTH_IX] != respDataLen)
    return -3;
  readLen = client.readBytes(response, respDataLen);
  if (readLen < respDataLen)
    return -4;
  for (int i = 0, j = 0; i < len; i++, j += 2) {
    regs[i] = response[j] * 256 + response[j + 1];
  }
  return 0;
}

int modbusWriteSingle(Client& client, unsigned int address, int val) {

  const byte CODE_IX = 7;
  const byte ERR_CODE_IX = 8;
  const byte RESPONSE_LENGTH = 9;

  client.setTimeout(3000);

  byte req[] = { 0, 1, 0, 0, 0, 6, 1, FNC_WRITE_SINGLE, // header
        (byte) (address / 256), (byte) (address % 256),
        (byte) (val / 256), (byte) (val % 256)};

  client.write(req, sizeof(req));

  byte response[RESPONSE_LENGTH];
  int readLen = client.readBytes(response, RESPONSE_LENGTH);
  if (readLen < RESPONSE_LENGTH)
    return MODBUS_NO_RESPONSE;
  switch (response[CODE_IX]) {
    case FNC_WRITE_SINGLE:
      break;
    case (FNC_ERR_FLAG | FNC_WRITE_SINGLE):
      return response[ERR_CODE_IX]; // 0x01, 0x02, 0x03, 0x04 or 0x11
    default:
      return -2;
  }
  while (client.read() != -1);
  return 0;
}
