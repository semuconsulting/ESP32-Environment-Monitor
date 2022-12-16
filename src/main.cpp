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

#include <main.h>

/********************************************************************
  Set current timestamp in JSON (ISO 8601) format
********************************************************************/
void setTimestamp(void)
{
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(timestamp, 25, JSONDATE, timeinfo);
}

/********************************************************************
  Check sensor status
********************************************************************/
bool checkIaqSensorStatus(void)
{
  if (bme680.status != BSEC_OK)
  {
    if (bme680.status < BSEC_OK)
    {
      Serial.printf("BSEC error code : %u", bme680.status);
      return false;
    }
    else
    {
      Serial.printf("BSEC warning code : %u", bme680.status);
    }
  }

  if (bme680.bme680Status != BME680_OK)
  {
    if (bme680.bme680Status < BME680_OK)
    {
      Serial.printf("BSEC error code : %u", bme680.bme680Status);
      return false;
    }
    else
    {
      Serial.printf("BSEC warning code : %u", bme680.bme680Status);
    }
  }
  return true;
}

#ifdef SHOW_MON
/********************************************************************
  Display output on serial monitor
********************************************************************/
void showmon(void)
{
  Serial.printf("\nRaw Temp %4.2f", bme680.rawTemperature);
  Serial.printf("\nRaw Humidity %4.2f", bme680.rawHumidity);
  Serial.printf("\nGas Resistance %4.2f", bme680.gasResistance);
  Serial.printf("\nTemp: %4.2f", bme680.temperature);
  Serial.printf("\nPressure: %4.2f", bme680.pressure / 100);
  Serial.printf("\nHumidity: %4.2f", bme680.humidity);
  Serial.printf("\nIAQ: %4.2f", bme680.staticIaq);
  Serial.printf("\nIAQ Accuracy: %i", bme680.iaqAccuracy);
  Serial.printf("\nCO2: %4.2f", bme680.co2Equivalent);
  Serial.printf("\nVOC: %4.2f\n", bme680.breathVocEquivalent);
}
#endif

#ifdef SHOW_OLED
// /********************************************************************
//   Display output on OLED screen
// ********************************************************************/
void showoled(void)
{
  display.fillScreen(BLACK);
  display.setCursor(0, 0);
  display.setTextColor(RED);
  display.printf("Temp: %3.2f", bme680.temperature);
  display.setTextColor(GREEN);
  display.printf("\nPressure: %4.1f", bme680.pressure / 100);
  display.setTextColor(BLUE);
  display.printf("\nHumidity: %4.2f", bme680.humidity);
  display.setTextColor(YELLOW);
  display.printf("\nIAQ: %4.2f", bme680.staticIaq);
  display.printf("\nIAQ Accuracy: %i", bme680.iaqAccuracy);
  display.setTextColor(MAGENTA);
  display.printf("\nCO2: %4.2f", bme680.co2Equivalent);
  display.setTextColor(CYAN);
  display.printf("\nVOC: %4.2f\n", bme680.breathVocEquivalent);
  display.setTextColor(WHITE);
  display.print(WiFi.localIP());
}
#endif

/********************************************************************
  Erase EEPROM sensor calibration data block
********************************************************************/
void eraseState(void)
{
  Serial.println(F("Erasing EEPROM"));

  for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
    EEPROM.write(i, 0);

  EEPROM.commit();
}

/********************************************************************
  Load sensor calibration data from EEPROM
********************************************************************/
bool loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE)
  {
    // Existing state in EEPROM
    Serial.println(F("Reading state from EEPROM"));

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
      bsecState[i] = EEPROM.read(i + 1);
      // Serial.println(bsecState[i], HEX);
    }

    bme680.setState(bsecState);
    if (!checkIaqSensorStatus())
    {
      return false;
    }
  }
  else
  {
    eraseState();
  }
  return true;
}

/********************************************************************
  Update sensor calibration data and store in EEPROM
********************************************************************/
bool updateState(void)
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0)
  {
    if (bme680.iaqAccuracy >= 3)
    {
      update = true;
      stateUpdateCounter++;
    }
  }
  else
  {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis())
    {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update)
  {
    bme680.getState(bsecState);
    if (!checkIaqSensorStatus())
    {
      return false;
    }

    Serial.println(F("Writing state to EEPROM"));
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
      EEPROM.write(i + 1, bsecState[i]);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
  return true;
}

/********************************************************************
  Read sensor and return reading struct
********************************************************************/
reading readSensor(void)
{
  reading bme;

  setTimestamp();

  strcpy(bme.time, timestamp);
  bme.temp = bme680.temperature;
  bme.pres = bme680.pressure / 100;
  bme.humy = bme680.humidity;
  bme.IAQ = bme680.staticIaq;
  bme.IAQacc = bme680.iaqAccuracy;
  bme.CO2 = bme680.co2Equivalent;
  bme.VOC = bme680.breathVocEquivalent;

  return bme;
}

/********************************************************************
  Convert sensor reading to timestamped json payload
  {"uptime":12345678,"time":"2018-04-30T16:00:13.000Z","temp":55.1,"pres":1005.4,"humy":55.6,"IAQ":180.6,"IAQaccuracy":3,"CO2":1580.6,"VOC":10.6}
********************************************************************/
void reading2json(char *json, reading &bme)
{
  StaticJsonDocument<SENSORCAP> doc;

  doc["uptime"] = millis() / 1000;
  doc["time"] = bme.time;
  doc["temp"] = bme.temp;
  doc["pres"] = bme.pres;
  doc["humy"] = bme.humy;
  doc["IAQ"] = bme.IAQ;
  doc["IAQacc"] = bme.IAQacc;
  doc["CO2"] = bme.CO2;
  doc["VOC"] = bme.VOC;

  serializeJson(doc, json, SENSORCAP);
}

/********************************************************************
  Convert sensor json payload to sensor reading
********************************************************************/
void json2reading(char *json, reading &bme)
{
  StaticJsonDocument<SENSORCAP> doc;
  deserializeJson(doc, json, SENSORCAP);

  strcpy(bme.time, doc["time"]);
  bme.temp = doc["temp"];
  bme.pres = doc["pres"];
  bme.humy = doc["humy"];
  bme.IAQ = doc["IAQ"];
  bme.IAQacc = doc["IAQacc"];
  bme.CO2 = doc["CO2"];
  bme.VOC = doc["VOC"];
}

/********************************************************************
 Handle REST GET request from client for latest sensor reading
********************************************************************/
void getReading(void)
{
  char *jsondata = new char[SENSORCAP];
  reading2json(jsondata, currReading);
  webserver.send(200, "application/json", jsondata);
  delete jsondata;
}

/********************************************************************
 Handle REST GET request from client for configuration settings
 {"sensorInt":3600000,"graphInt":3600000,"logInt":3600000}
********************************************************************/
void getConfig(void)
{
  char *jsondata = new char[CONFIGCAP];
  StaticJsonDocument<CONFIGCAP> doc;

  doc["sensorInt"] = sensorInterval;
  doc["graphInt"] = graphInterval;
  doc["logInt"] = logInterval;

  serializeJson(doc, jsondata, CONFIGCAP);
  webserver.send(200, "application/json", jsondata);
  delete jsondata;
}

/********************************************************************
 Handle REST PUT request from client to update configuration
 {"sensorInt":3600000,"graphInt":3600000,"logInt":3600000}
********************************************************************/
void putConfig(void)
{

  StaticJsonDocument<CONFIGCAP> doc;

  if (webserver.args() == 0)
  {
    Serial.print(F("putConfig failed - no arguments received"));
    webserver.send(500, "text/plain", "MISSING ARGS");
    return;
  }

  DeserializationError derr = deserializeJson(doc, webserver.arg(0));
  if (derr)
  {
    Serial.print(F("putConfig json parsing failed: "));
    Serial.println(derr.c_str());
    webserver.send(500, "text/plain", "INVALID JSON");
    return;
  }

  // Validate input parms
  if (doc["sensorInt"] < 1000 || doc["graphInt"] < 1000 || doc["logInt"] < 1000 || doc["logInt"] > 99999000)
  {
    Serial.print(F("putConfig failed - arguments invalid"));
    webserver.send(500, "text/plain", "INVALID ARGS");
    return;
  }

  sensorInterval = doc["sensorInt"];
  graphInterval = doc["graphInt"];
  logInterval = doc["logInt"];
  Serial.printf("Sensor Refresh Interval = %u", sensorInterval);
  Serial.printf("Graph Refresh Interval = %u", graphInterval);
  Serial.printf("Log File Refresh Interval = %u", logInterval);
  webserver.send(200, "text/plain", "SUCCESS");
}

/********************************************************************
 Handle REST GET request from client for sensor log data
********************************************************************/
void getLog(void)
{
  char *jsondata = new char[SENSORCAP];
  String log;

  log = LOGFILE_HDR;
  for (uint16_t i = 0; i < LOGSIZE; i++)
  {
    reading2json(jsondata, sensorLog[i]);
    log += jsondata;
    if (i < LOGSIZE - 1)
    {
      log += ",";
    }
  }
  log += LOGFILE_TLR;
  webserver.send(200, "application/json", log);
  delete jsondata;
}

/********************************************************************
  Update in-memory sensor log with latest reading
********************************************************************/
void updateLog(void)
{
  for (uint16_t i = 0; i < LOGSIZE - 1; i++)
  {
    sensorLog[i] = sensorLog[i + 1]; // Shift older readings down
  }
  sensorLog[LOGSIZE - 1] = currReading; // TODO ideally average reading over inter-log period

  saveLog();
}

/********************************************************************
  Save in-memory sensor log file to SPIFFS
********************************************************************/
bool saveLog()
{
  char *jsondata = new char[SENSORCAP];

  SPIFFS.remove(LOGFILE);

  File file = SPIFFS.open(LOGFILE, FILE_APPEND);
  if (!file)
  {
    Serial.println(F("Error opening Logfile"));
    return false;
  }
  if (!file.print(LOGFILE_HDR))
  {
    Serial.println(F("Logfile header write failed"));
    return false;
  }

  for (uint16_t i = 0; i < LOGSIZE; i++)
  {
    reading2json(jsondata, sensorLog[i]);
    if (!file.print(jsondata))
    {
      Serial.println(F("Logfile sensor write failed"));
      return false;
    }

    if (i < LOGSIZE - 1)
    {
      if (!file.print(","))
      {
        Serial.println(F("Logfile comma write failed"));
        return false;
      }
    }
  }

  if (!file.print(LOGFILE_TLR))
  {
    Serial.println(F("Logfile trailer write failed"));
    return false;
  }

  file.close();
  lastLog = millis();
  Serial.println(F("Logfile updated"));
  delete jsondata;
  return true;
}

/********************************************************************
  Load in-memory sensor log from SPIFFS.
********************************************************************/
bool loadLog(void)
{

  Serial.println(F("Reloading Logfile..."));
  uint16_t i = 0;
  StaticJsonDocument<LOGCAP> doc;

  File file = SPIFFS.open(LOGFILE, FILE_READ);
  if (!file)
  {
    Serial.println(F("Error opening Logfile"));
    return false;
  }

  file.find(LOGFILE_HDR);
  do
  {
    DeserializationError derr = deserializeJson(doc, file);
    if (derr)
    {
      Serial.print(F("Logfile JSON parsing failed: "));
      Serial.println(derr.c_str());
      return false;
    }

    strcpy(sensorLog[i].time, doc["time"]);
    sensorLog[i].temp = doc["temp"];
    sensorLog[i].pres = doc["pres"];
    sensorLog[i].humy = doc["humy"];
    sensorLog[i].IAQ = doc["IAQ"];
    sensorLog[i].IAQacc = doc["IAQacc"];
    sensorLog[i].CO2 = doc["CO2"];
    sensorLog[i].VOC = doc["VOC"];
    i++;

  } while (file.findUntil(",", "]")); // Get next json chunk in file

  file.close();
  Serial.println(F("Sensor log reloaded from logfile"));
  return true;
}

/********************************************************************
   Webserver - handle request for root web page (index.html)
********************************************************************/
void handleRoot()
{
  webserver.sendHeader("Location", HTMLFILE, true);
  webserver.send(302, "text/html", "");
}

/********************************************************************
  Webserver - handle all other web requests by attempting to load 
  relevant resource from SPIFFS file system
********************************************************************/
bool handleWebRequests()
{

  if (loadFromSpiffs(webserver.uri()))
  {
    return true;
  }

  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += webserver.uri();
  message += "\nMethod: ";
  message += (webserver.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webserver.args();
  message += "\n";
  for (uint8_t i = 0; i < webserver.args(); i++)
  {
    message += " NAME:" + webserver.argName(i) + "\n VALUE:" + webserver.arg(i) + "\n";
  }
  webserver.send(404, "text/plain", message);
  return false;
}

/********************************************************************
  Webserver - load web resource (html, css, js, json, etc.) from ESP32
  SPIFFS flash file system and stream response with appropriate dataType
********************************************************************/
bool loadFromSpiffs(String path)
{

  String dataType = "text/plain";

  if (path.endsWith("/"))
  {
    path += "index.html";
  }
  else if (path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }
  else if (path.endsWith(".html"))
  {
    dataType = "text/html";
  }
  else if (path.endsWith(".htm"))
  {
    dataType = "text/html";
  }
  else if (path.endsWith(".css"))
  {
    dataType = "text/css";
  }
  else if (path.endsWith(".js"))
  {
    dataType = "application/javascript";
  }
  else if (path.endsWith(".png"))
  {
    dataType = "image/png";
  }
  else if (path.endsWith(".gif"))
  {
    dataType = "image/gif";
  }
  else if (path.endsWith(".jpg"))
  {
    dataType = "image/jpeg";
  }
  else if (path.endsWith(".ico"))
  {
    dataType = "image/x-icon";
  }
  else if (path.endsWith(".xml"))
  {
    dataType = "text/xml";
  }
  else if (path.endsWith(".pdf"))
  {
    dataType = "application/pdf";
  }
  else if (path.endsWith(".zip"))
  {
    dataType = "application/zip";
  }
  else if (path.endsWith(".json"))
  {
    dataType = "application/json";
  }

  if (webserver.hasArg("download"))
  {
    dataType = "application/octet-stream";
  }

  File file = SPIFFS.open(path.c_str(), FILE_READ);
  if (!file)
  {
    Serial.print(F("Error opening file: "));
    Serial.println(path.c_str());
    return false;
  }

  if (webserver.streamFile(file, dataType) != file.size())
  {
    Serial.print(F("Error streaming file: "));
    Serial.println(path.c_str());
  }

  file.close();
  return true;
}

/********************************************************************
  Initialise BME680 sensor
********************************************************************/
bool init_bme680(void)
{
  bme680.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  Serial.printf("\nBSEC library version %u.%u.%u.%u\n", bme680.version.major, bme680.version.minor, bme680.version.major_bugfix, bme680.version.minor_bugfix);

  checkIaqSensorStatus();
  bme680.setConfig(bsec_config_iaq);
  if (!checkIaqSensorStatus())
  {
    return false;
  }

  loadState(); //  attempt to load calibration state from EEPROM

  bsec_virtual_sensor_t sensorList[10] = {
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_STATIC_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  bme680.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  if (!checkIaqSensorStatus())
  {
    return false;
  }
  return true;
}

/********************************************************************
  Initialise flash file system
********************************************************************/
bool initSPIFFS()
{

  if (!SPIFFS.begin(true))
  { // Format SPIFFS if mount fails
    Serial.println(F("SPIFFS Mount Failed"));
    return false;
  }

  Serial.println(F("SPIFFS Mounted OK"));
  return true;
}

/********************************************************************
  Connect to WiFi using credentials defined in wificreds.h
********************************************************************/
bool initWiFi()
{

  uint8_t retries = 0;

  WiFi.begin(ssid, password);
  Serial.println(F("Connecting to WiFi..."));

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && retries++ < 20)
  {
    delay(500);
    Serial.print(".");
  }

  if (retries == 20)
  {
    Serial.println(F("\nFailed to connect to WiFi"));
    return false;
  }

  if (WiFi.localIP() == IPAddress(0, 0, 0, 0))
  {
    return false;
  }

  // If connection successful, show assigned IP address
  Serial.printf("\nConnected to %s", ssid);
  Serial.print(F("\nIP address: "));
  Serial.println(WiFi.localIP());
  return true;
}

/********************************************************************
  Webserver - define URI handlers and initialize webserver
********************************************************************/
bool initWebserver()
{

  webserver.on("/", handleRoot);                 // Function called for root web page (index.html)
  webserver.on("/sensor", HTTP_GET, getReading); // Function called when client requests sensor data
  webserver.on("/config", HTTP_GET, getConfig);  // Function called when client requests config data
  webserver.on("/config", HTTP_PUT, putConfig);  // Function called when client updates system config data
  webserver.on("/log", HTTP_GET, getLog);        // Function called when client requests log data
  webserver.onNotFound(handleWebRequests);       // Function called for all other web requests
  webserver.begin();
  delay(1000); // Wait till webserver stabilises
  Serial.println(F("Webserver started"));

  return true; // For consistency with other init() functions
}

/********************************************************************
  Set system time from NTP server
********************************************************************/
bool initTime()
{

  const char *NTPSERVER = "pool.ntp.org";
  const long GMTOFFSET = 0; // time offset from GMT in seconds
  const int DSTOFFSET = 0;  // daylight saving time offset in seconds
  struct tm timeinfo;
  uint8_t retries = 0;
  int ts = 0;

  // Get time from NTP - up to 3 attempts at 500ms intervals
  Serial.print(F("Attempting to get time from NTP"));
  configTime(GMTOFFSET, DSTOFFSET, NTPSERVER);
  while (!ts && retries++ < 3)
  {
    ts = getLocalTime(&timeinfo);
    delay(500);
    Serial.print(".");
  }

  if (!ts)
  {
    Serial.println("");
    Serial.println(F("Failed to set system time from NTP"));
    return false;
  }

  Serial.println("");
  Serial.println(F("System time set from NTP"));

  return true;
}

/********************************************************************
  Initialise logfile with nominal readings
********************************************************************/
void initLog(void)
{
  setTimestamp();

  for (uint16_t i = 0; i < LOGSIZE; i++)
  {
    strcpy(sensorLog[i].time, timestamp);
    sensorLog[i].temp = 20.0;
    sensorLog[i].pres = SL;
    sensorLog[i].humy = 50.0;
    sensorLog[i].IAQ = 100.0;
    sensorLog[i].IAQacc = 0;
    sensorLog[i].CO2 = 500.0;
    sensorLog[i].VOC = 0.5;
  }

  saveLog();
}

/********************************************************************
  Set and display initialisation error state
********************************************************************/
void setErr(const char *msg)
{

  Serial.println(F(msg));
#ifdef SHOW_OLED
  display.fillScreen(BLACK);
  display.setCursor(0, 20);
  display.setTextColor(RED);
  display.println(msg);
#endif
  active = false;
}

/********************************************************************
  Setup and initialisation routines
********************************************************************/
void setup(void)
{

  active = true;
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length
  Serial.begin(115200);
  Wire.begin();

#ifdef INIT_EEPROM
  eraseState();
#endif

#ifdef SHOW_OLED
  display.begin();
  display.fillScreen(BLACK);
  display.setCursor(0, 24);
  display.setTextColor(WHITE);
  display.println(F("Initialising..."));
#endif

  Serial.println(F("BME680 initialisation..."));
  if (!init_bme680() && active)
  {
    setErr("BME680 initialisation failed");
  }

  if (active)
  {
    Serial.println(F("SPIFFS initialisation..."));
    if (!initSPIFFS())
    {
      setErr("SPIFFS initialisation failed");
    }
  }

  if (active)
  {
    Serial.println(F("WiFI initialisation..."));
    if (!initWiFi())
    {
      setErr("\nWiFi initialisation failed");
    }
  }

  if (active)
  {
    Serial.println(F("Web Server initialisation..."));
    if (!initWebserver())
    {
      setErr("\nWeb Server initialisation failed");
    }
  }

  if (active)
  {
    Serial.println(F("NTP Time initialisation..."));
    if (!initTime())
    {
      setErr("\nNTP Time initialisation failed");
    }
  }

  if (active)
  {
    Serial.println(F("Loading log from file..."));
    if (!loadLog())
    {
      Serial.println(F("No log file available - initialising log"));
      initLog();
    }
  }

  if (active)
  {
    Serial.println(F("Initialisation Complete!"));
  }
}

/********************************************************************
  Main loop
********************************************************************/
void loop(void)
{

  if (active)
  {
    if (bme680.run())
    { // If new data is available

      currReading = readSensor();
      updateState();
#ifdef SHOW_MON
      showmon(); // Display on serial monitor
#endif
#ifdef SHOW_OLED
      showoled(); //  Display on OLED
#endif

      // Update log file periodically
      if ((millis() - lastLog) > logInterval)
      {
        updateLog();
        lastLog = millis();
      }
    }
    else
    {
      checkIaqSensorStatus();
    }

    webserver.handleClient(); // Handle web clients
  }
}
