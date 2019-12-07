/* neute Version mit Z19B CO2 - Daten auszulesen
       MH-Z19B mißt alle x Sekunden (x = frequenz)
   Include ESP32_Z19_Include enthält alle Includes 
   Include ESP32_Z19_Config enthält alle Parameter
   -> so wird das Hauptprogramm übersichtlicher
   Funktion:
       Alle x Sekunden - Daten (CO2 und Temperatur) messen und
       über WLAN / MQTT nach FHEM senden.
   Hardware:
       ESP32 Lolin D32
       Sensor MH-Z19B 0--5000 ppm
   Verkabelung:
       GND an GND / USB an Vin 
       Pin 21 (SDA) und Pin 22 (SCL) an TX bzw. RX
   MH-Z19B - Funktionen:
       myMHZ19.printCommunication(boolean, boolean) .. (true/false) zur Fehlersuche (true/false) als HEX-Code
       myMHZ19.autoCalibration(bollean)             .. (true/false) auf 400 ppm calibrieren
       myHZZ19.setRange(value)                      .. (1000/2000/3000/5000) Empfindlichkeit
       myMHZ19.setSpan(num)                         ..
       myMHZ19.recoveryReset()                      ..
       myMHZ19.getVersion(char array[])             .. returns version number e.g 02.11
       myMHZ19.getRange()
       myMHZ19.getBackgroundCO2()
       myMHZ19.getTempAdjustment()      */

#include <Arduino.h>
#include <WiFiClientSecure.h>   // superset of WiFiClient for secure connections using TLS (SSL)
#include <WiFiServer.h>         // Creates a server that listens for incoming connections (here for cerdentials)
#include <HTTPUpdate.h>
#include <Preferences.h>        // WiFi storage NVS (non-Volatile storage)
#include <time.h>               // Time and Date for checking the last update
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "MHZ19.h"              // include main library for MH-Z19B - Sensor
//#include <Wire.h>             // This library allows you to communicate with I2C

#include "ESP32_Z19B_Config.h"  // All Variables and Defines

WiFiServer NVS_Server(80);      // server for Webpage to enter the credentials if not stored in NVS
WiFiClientSecure OTA_Client;    // client for OTA - download
Preferences preferences;        // declare class object
WiFiClient espClient;           // create an instance of PubSubClient client local MQTT
PubSubClient FHEM_Client(espClient);
MHZ19 myMHZ19;                  // Constructor for MH-Z19 class - Sensor
HardwareSerial Z19_serial(1);   // Define Z19_serial as serial - input 1 for the sensor
StaticJsonDocument<400> doc;
JsonObject MQTT_ATT = doc.to<JsonObject>();



#include "ESP32_Z19B_Include.h"      // All Includes

#if MQTT_MAX_PACKET_SIZE < 256  // If the max message size is too small, throw an error at compile time. See PubSubClient.cpp line 359
#error "MQTT_MAX_PACKET_SIZE is too small in libraries/PubSubClient/src/PubSubClient.h at line 359, increase it to 256"
#endif

void setup()
{
  Serial.begin(115200);                   // For ESP32 baudrate is 115200 etc.
  /* Write Headerinformation into Console */
  Serial.printf("Programname = %s\n", Programname);
  Serial.printf("ESP_ChipRevision() = %u\n", ESP.getChipRevision());
  Serial.printf("ESP_CpuFreqMHz()   = %u\n", ESP.getCpuFreqMHz());

  // Check, if credetials are stored in NVS-Storage (non vulen
  preferences.begin("wifi", false);                            // NVS- storage
  // preferences.clear();                                      // to reset the NVS-Storage for testing
  wifiSSID =  preferences.getString("ssid", "none");           // NVS key ssid
  wifiPassword =  preferences.getString("password", "none");   // NVS key password
  OTA_day =  preferences.getUInt("OTA", 0 );                   // NVS day of the year e.g. 313 for 10.11.
  preferences.end();

  WiFi.onEvent(ServerEvent);
  while (wifiSSID == "none") {
    Serial.println("bitte Credentials pflegen");
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(AP_SSID);
    Serial.println("AP Started");
    Serial.printf("AP SSID: %s\n", AP_SSID);
    Serial.printf("AP IPv4: %s\n", WiFi.softAPIP());
    NVS_Server.begin();
    delay(1000);
    getCredentials();
    delay(100);
    /* Without WiFi-Credentials stay in While */
    Serial.print("Stored SSID: ");
    Serial.println(wifiSSID);
  }
  Serial.println("WiFi disconnect(true)");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_MODE_STA);                    //close AP network
  delay(100);
  Serial.println("WiFi enableSTA(true)");
  WiFi.enableSTA(true);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  WiFi.setHostname(STA_SSID);                 // hast to be just after WiFi.Begin
  Serial.println(WiFi.getHostname());
  delay(100);
  while (WiFi.status() != WL_CONNECTED) {     // attempt to connect to Wifi network:
    Serial.print(".");
    delay(1000);                              // wait 1 second for re-trying
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }
  delay(100);
  Serial.println("Wifi connected");

  Check_Last_OTA();       // should be at least yesterday -> one update per day is enought

  if (wifi_connected and !OTA_checked) {
    OTA_Client.setCACert(rootCACertificate);
    httpUpdate.setLedPin(LED_BUILTIN, HIGH);
    t_httpUpdate_return ret = httpUpdate.update(OTA_Client, serverOTA, 443, "/fota/ESP32_Z19B_09.bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        OTA_checked = true;
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        OTA_checked = true;
        break;
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        OTA_checked = true;
        break;
    }
  }
  FHEM_Client.setClient(espClient);
  FHEM_Client.setServer(mqtt_server, 1883);
  if (!FHEM_Client.connected()) {        // if client was disconnected then try to reconnect again
    fhemconnect();
  }
  sprintf(mqtt_topic, "/ESP32/%lX%lX", (long unsigned int)(0xffff & ESP.getEfuseMac()), (long unsigned int)(0xffff & (ESP.getEfuseMac() >> 32)));

  MQTT_ATT["01.Progr."] = Programname;
  MQTT_ATT["02.Bord"]   = "LOLIN D32";
  sprintf(ChipID, "%lX%lX", (long unsigned int)(0xffff & ESP.getEfuseMac()), (long unsigned int)(0xffff & (ESP.getEfuseMac() >> 32)));
  MQTT_ATT["03.ChipID"] = ChipID;
  MQTT_ATT["03a_Rev."]  = ESP.getChipRevision();
  MQTT_ATT["03b_Freq."] = ESP.getCpuFreqMHz();
  MQTT_ATT["04.Ver."]   = ESP.getSdkVersion();
  serializeJson(MQTT_ATT, mqtt_msg, sizeof(mqtt_msg));
  Serial.printf("MQTT-Message1 = %s\n", mqtt_msg);
  FHEM_Client.publish(mqtt_topic, mqtt_msg);
  FHEM_Client.setCallback(mqttcallback);
  delay(100);

  Z19_serial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(500);
  myMHZ19.begin(Z19_serial);            // Important, Pass your Stream reference
  delay(500);

  myMHZ19.autoCalibration(sens_autoCalibratation);
  setRange(sens_Range );                // set Range 2000 using a function, see below (disabled as part of calibration)
  verifyRange(sens_Range );             // Check if 2000 is set as range     3000 -> gibt fehler

  //  Second Information block
  myMHZ19.getVersion(myVersion);
  for (byte i = 0; i < 4; i++)
  {
    SensVersion = SensVersion + myVersion[i];
    if (i == 1) {
      SensVersion = SensVersion + ".";
    }
  }
  doc.clear();
  MQTT_ATT["10.Sensor"]  = "MH-Z19B";
  MQTT_ATT["11.Version"] = SensVersion ;
  MQTT_ATT["12.BG_CO2"]  = myMHZ19.getBackgroundCO2();
  MQTT_ATT["13.BG_Temp."] = myMHZ19.getTempAdjustment();
  MQTT_ATT["14.UpdFreq."] = frequenz;
  MQTT_ATT["15.AutoCal."] = sens_autoCalibratation;
  MQTT_ATT["16.Range"]  = sens_Range;
  serializeJson(MQTT_ATT, mqtt_msg);
  Serial.printf("MQTT-Message2 = %s\n", mqtt_msg);
  FHEM_Client.publish(mqtt_topic, mqtt_msg);
  FHEM_Client.setCallback(mqttcallback);
  delay(100);
  doc.clear();
  Serial.println("-------------------------------");
}

void loop() {
  FHEM_Client.loop();

  if (millis() - getDataTimer >= (frequenz * 1000))     // Check if interval has elapsed (non-blocking delay() equivilant)
  {
    int CO2 = myMHZ19.getCO2(true, true);               // Request CO2 (as ppm) unlimimted value, new request
    delay(100);
    doc.clear();
    MQTT_ATT["CO2"] = CO2;

    float Temp = myMHZ19.getTemperature(true, false);   // decimal value, new request (false = not new request
    MQTT_ATT["Temperature"] = Temp;
    // MQTT_ATT["Transmittance"] = myMHZ19.getTransmittance();
    MQTT_ATT["Temp.offset"] = myMHZ19.getTemperatureOffset();
    
    // double adjustedCO2 = myMHZ19.getCO2Raw();
    // MQTT_ATT["Raw CO2"] = adjustedCO2;
    // adjustedCO2 = 6.60435861e+15 * exp(-8.78661228e-04 * adjustedCO2);      // Exponential equation for Raw & CO2 relationship
    // MQTT_ATT["Adjusted CO2"] = adjustedCO2;

    serializeJson(MQTT_ATT, mqtt_msg);
    Serial.printf("MQTT-Reading = %s\n", mqtt_msg);
    if ( !FHEM_Client.connected() ) {
      fhemconnect();
    }
    FHEM_Client.publish(mqtt_topic, mqtt_msg);
    delay(100);
    doc.clear();
    getDataTimer = millis();        // Update interval
  }
}
