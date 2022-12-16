/**
ESP32 Indoor Air Quality (IAQ) Monitor Project.

https://github.com/semuconsulting/esp32_IAQ_monitor.

Designed for ESP32 Devkit4 MCU and BME680 I2C sensor, with option to
display output on SSD1331 RGB SPI OLED in addition to dynamic web page.

Uses Bosch SensorTec Arduino library: https://github.com/BoschSensortec/BSEC-Arduino-library.

BME680 processing based on original example here: https://github.com/BoschSensortec/BSEC-Arduino-library/tree/master/examples/basic_config_state.

Original license here: https://github.com/BoschSensortec/BSEC-Arduino-library/blob/master/LICENSE.

(c) SEMU Consulting 2019 - BSD 3-Clause License
*/

#ifndef _ESP32IAQ_MAIN_H_
#define _ESP32IAQ_MAIN_H_

// (Un)comment these to show(hide) sensor readings on OLED screen and/or serial monitor
#define SHOW_OLED
#define SHOW_MON

// Uncomment this to initialise EEPROM
// #define INIT_EEPROM

#include <EEPROM.h>
#include "bsec.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h>
/*
   wificreds.h file should contain your local WiFi connection credentials
   in the form:
   const char* ssid = "...";
   const char* password =  "...";
*/
#include <wificreds.h>

#ifdef SHOW_OLED

#include <Adafruit_SSD1331.h>

// SSD1331 SPI definitions for ESP32 Devkit4
const uint8_t sclk = 18;
const uint8_t mosi = 23;
const uint8_t cs = 5;
const uint8_t rst = 16;
const uint8_t dc = 17;

// Color definitions
const uint32_t BLACK = 0x0000;
const uint32_t BLUE = 0x001F;
const uint32_t RED = 0xF800;
const uint32_t GREEN = 0x07E0;
const uint32_t CYAN = 0x07FF;
const uint32_t MAGENTA = 0xF81F;
const uint32_t YELLOW = 0xFFE0;
const uint32_t WHITE = 0xFFFF;

#endif

// Bsec BME680 configuration state definitions
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};
const uint32_t STATE_SAVE_PERIOD = (360 * 60 * 1000); // 360 minutes - 4 times a day
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;

// Constant declarations
// Default = 5 day log at refresh rate of 0.5 hour (1,800,000 ms)
// NB: will need to erase flash if LOGSIZE is changed
const float SL = 1013.25;                   // nominal sea level pressure
const uint8_t LOGSIZE = 240;                // maximum number of log file entries
const uint8_t SENSORCAP = 192;             // sensor buffer capacity, from ArduinoJson Assistant 6
const uint8_t LOGCAP = 216;           // logfile buffer capacity, from ArduinoJson Assistant 6 (192 + 24 margin)
const uint8_t CONFIGCAP = 80;         // config buffer capacity, from ArduinoJson Assistant 6 (48 + 32 margin)
const char *HTMLFILE = "/index.html";
const char *JSONDATE = "%FT%T.000Z";        // ISO8601 time format e.g. "2018-04-30T16:00:13.000Z"
const char *LOGFILE = "/logfile.json";      // logfile name
const char *LOGFILE_HDR = "{\"logfile\":["; // logfile JSON header
const char *LOGFILE_TLR = "]}";             // logfile JSON trailer

// Variable declarations
char *timestamp = new char[25](); // container for ISO8601 timestamp
uint32_t sensorInterval = 5000;   // client sensor update interval ms
uint32_t graphInterval = 60000;   // client graph update interval ms
uint32_t logInterval = 1800000;   // server Log update interval ms
uint32_t lastLog = 0;
uint32_t lastSensor = 0;
bool active; // set to true when all initialisations passed

typedef struct
{
  char time[25];
  float temp;
  float pres;
  float humy;
  float IAQ;
  uint8_t IAQacc;
  float CO2;
  float VOC;
} reading;

reading currReading;              // current sensor reading
reading sensorLog[LOGSIZE] = {0}; // in-memory log of sensor readings

// Function declarations
bool init_bme680(void);
reading readSensor(void);
void setTimestamp(void);
void reading2json(char *json, reading &reading);
void json2reading(char *json, reading &reading);
bool checkIaqSensorStatus(void);
void eraseState(void);
bool loadState(void);
bool updateState(void);
void updateLog(void);
void initLog(void);
bool saveLog(void);
bool loadLog(void);
void showterm(void);
void showoled(void);
void getReading(void);
void getConfig(void);
void putConfig(void);
void getLog(void);
void handleRoot(void);
bool handleWebRequests(void);
bool loadFromSpiffs(String path);
bool initSPIFFS(void);
bool initWiFi(void);
bool initWebserver(void);
bool initTime(void);
void setErr(const char *msg);

// Instantiate SSD1331 OLED class if required
#ifdef SHOW_OLED
Adafruit_SSD1331 display = Adafruit_SSD1331(cs, dc, mosi, sclk, rst);
#endif

// Instantiate Bsec Sensor class
Bsec bme680;

// Instantiate WebServer class
WebServer webserver(80);

#endif
