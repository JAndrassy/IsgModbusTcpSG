# IsgModbusTcpSG

This Arduino sketch is for DIY "SG Ready" adapter for Stiebel Eltron ISGweb. The ISGweb device is Internet Service Gateway for Stiebel Eltron heat pumps. The ISGweb doesn't have physical SG Ready input pins, but SG Ready inputs can be be controlled over Modbus TCP. This sketch reads a state of digital input pin of Arduino and sets the state over Modbus TCP to SG Ready INPUT1 register of ISGweb.

I coded this sketch for a fellow Fronius Symo Hybrid owner from German Photovoltaikforum. The collaboration started with [this forum thread](https://www.photovoltaikforum.com/thread/139478-sg-ready-heizung-via-modbus-ansteuern/?pageNo=3) (in German, but with pictures). Here is [the basic version of the sketch](https://github.com/jandrassy/lab/blob/master/IsgModbusTcpSG/IsgModbusTcpSG.ino).

The basic function of the sketch is very simple, but in the spirit of "If it's worth doing, it's worth overdoing", I reused everything I could from my Regulator project. So the final sketch has monitoring over Serial Monitor, Telnet and web page, it logs the state in csv files and it has a status LED and a beeper.

## Hardware

The sketch was developed first on classic Arduino Nano with enc28j60 ethernet shield. To add SD card for logging and static web files we had to move to Nano Every which has 48 kB of flash (16 kB more then classic Nano) and 6 kB SRAM (3 times more then classic Nano.) The enc28j60 shield is compatible with Nano Every, but it turned out not be compatible with additional SPI device (SD card) so it needed [a modification](https://github.com/UIPEthernet/UIPEthernet/tree/master/hardware#nano-ethernet-shield).

The only MCU specific code is the Watchdog setup for the ATmega4809 of Nano Every. The rest of the code should be portable to other Arduino model and other networking shield or module with proper Arduino networking library. 

## Source code
To use the sketch, copy the folder `IsgModbusTcpSG`from this GitHub repository into your sketch folder of Arduino IDE.

### System
* IsgModbusTcpSG.ino - global variables, setup() and loop(), handling network and MODBUS, main functions
* SymoRTC.ino - reads time over MODBUS from DataManager of Symo photovoltaics inverter.
* Events.ino - data saved to EEPROM and logged to SD file for monitoring

### Front panel
* Beeper.ino - handles a buzzer. In loop handles the alarm sound if the system is in alarm state.
* StatusLed.ino - shows state with plain LED 

### Monitoring
* Terminal.ino - logging to Serial Monitor or to Telnet client and reading simple commands 
* CsvLog.ino - logging state changes to csv files
* WebServer.ino - JSON data for the web pages and serving of static web page files and csv files from SD card

### Web interface

The `data` subfolder contains static files of the web page. The static web pages use data in json format requested from the WebServer implemented in WebServer.ino.

WebServer.ino can serve the pages from SD card, but this static web pages can be started from a folder on a computer to show the data from Arduino. Only set the IP address of Arduino in script.js.

