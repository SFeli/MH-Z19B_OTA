/* neunte Version mit Z19B CO2 - Daten auszulesen
  Include:
  ServerEvent(WiFiEvent_t event)
      Handler für WiFiEvent
  String urlDecode(const String& text)
  mqttcallback(char* topic, byte* message, unsigned int length)
      Handler für MQTT-Callback - Empfang von MQTT-Meldungen
*/
#include <Arduino.h>

void ServerEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_AP_START:               // Event 13
      WiFi.softAPsetHostname(AP_SSID);        // can set ap hostname here
      WiFi.softAPenableIpV6();                // enable ap ipv6 here
      break;
    case SYSTEM_EVENT_STA_START:              // Event 2
      WiFi.setHostname(STA_SSID);             // set sta hostname here
      Serial.print("STA Start ");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:          // Event 4
      Serial.print("STA Connected to AP!");
      WiFi.enableIpV6();                      //enable sta ipv6 here
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      // both interfaces get the same event
      Serial.print("STA IPv6: ");
      Serial.println(WiFi.localIPv6());
      Serial.print("AP IPv6: ");
      Serial.println(WiFi.softAPIPv6());
      break;
    case SYSTEM_EVENT_STA_GOT_IP:               // Event 7
      Serial.println("STA Got IP");
      Serial.print("STA SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("STA IPv4: ");
      Serial.println(WiFi.localIP());
      Serial.print("STA IPv6: ");
      Serial.println(WiFi.localIPv6());
      wifi_connected = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:         // Event 5
      wifi_connected = false;
      Serial.println("STA Disconnected");
      break;
    case SYSTEM_EVENT_ETH_START:               // Event 20
      Serial.println("ETH_START");
      break;
    default:
      break;
  }
}

String urlDecode(const String& text)
{
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len)
  {
    char decodedChar;
    char encodedChar = text.charAt(i++);
    if ((encodedChar == '%') && (i + 1 < len))
    {
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);
      decodedChar = strtol(temp, NULL, 16);
    }
    else {
      if (encodedChar == '+')
      {
        decodedChar = ' ';
      }
      else {
        decodedChar = encodedChar;  // normal ascii char
      }
    }
    decoded += decodedChar;
  }
  return decoded;
}

void mqttcallback(char* topic, byte* message, unsigned int length) {
  // Serial.print("Message arrived on topic: ");
  // Serial.print(topic);
  String messageTemp;
  for (int i = 0; i < length; i++) {
    //  Serial.print((char)message[i]);
    messageTemp += (char)message[i];
    mqtt_msg[i] = (char)message[i];
  }
  // Serial.println();

  if (strcmp(topic, "/ESP32/7D80806A/upd") == 0) {
    //  Serial.print("FHEM_MQTT received Topic 7D80806A ...");
    preferences.begin("wifi", false);           // Note: Namespace name is limited to 15 chars
    //  Serial.println("Writing OTA_Day - 1 into NVS");
    preferences.putUInt("OTA", OTA_day - 1);
    delay(300);
    preferences.end();
    ESP.restart();
  }
  if (strcmp(topic, "/ESP32/7D80806A/cmd") == 0) {
    //  Serial.print("FHEM_MQTT received Topic 7D80806A/cmd ...");
    String tempmsg = "Command arrived: " + messageTemp;
    FHEM_Client.publish(mqtt_topic, (char*) tempmsg.c_str());
    doc.clear();
    // delay(300);
    if (messageTemp == "getCO2") {
      int CO2 = myMHZ19.getCO2(false, false);  // Request CO2 (as ppm) limimted value, last measurement
      //  delay(100);
      MQTT_ATT["CO2_0_0"] = CO2;
      CO2 = myMHZ19.getCO2(true, false);  // Request CO2 (as ppm) unlimimted value, last measurement
      //  delay(100);
      MQTT_ATT["CO2_1_0"] = CO2;
      CO2 = myMHZ19.getCO2(true, true);   // Request CO2 (as ppm) unlimimted value, new request
      MQTT_ATT["CO2"] = CO2;              // is the best value -> put it into graph
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    if (messageTemp ==  "getCO2Raw") {
      int CO2 = myMHZ19.getCO2Raw(false);  // Request CO2 (as ppm) "raw" CO2 value of unknown units
      MQTT_ATT["CO2_0"] = CO2;
      CO2 = myMHZ19.getCO2(true);  // Request CO2 (as ppm) "raw" CO2 value of unknown units
      MQTT_ATT["CO2_1"] = CO2;
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    if (messageTemp ==  "getRange") {
      int CO2 = myMHZ19.getRange();       // reads range using command 153
      MQTT_ATT["16.Range"] = CO2;         // update the Headerinformations 16.Range
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    if (messageTemp == "getAccuracy") {
      int CO2 = myMHZ19.getAccuracy(false);  // Returns accuracy value if available
      //  delay(100);
      MQTT_ATT["Accuracy_0"] = CO2;
      CO2 = myMHZ19.getAccuracy(true);  // Returns accuracy value if available
      //  delay(100);
      MQTT_ATT["Accuracy_1"] = CO2;
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    if (messageTemp == "ABC_off") {
      myMHZ19.autoCalibration(false);  // 121 0x79 Turns ABC logic on or off (b[3] == 0xA0 - on, 0x00 - off)
      delay(100);                      // set ABC off and check the value
      int CO2 =  myMHZ19.getABC();     // 125 0x7D Returns ABC logic status (1 - enabled, 0 - disabled)
      MQTT_ATT["15.AutoCal."] = CO2;
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    if (messageTemp == "ABC_on") {
      myMHZ19.autoCalibration(true);  // 121 0x79 Turns ABC logic on or off (b[3] == 0xA0 - on, 0x00 - off)
      delay(100);                     // set ABC on and check the value
      int CO2 =  myMHZ19.getABC();    // 125 0x7D Returns ABC logic status (1 - enabled, 0 - disabled)
      MQTT_ATT["15.AutoCal."] = CO2;
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    if (messageTemp == "Zero") {
      myMHZ19.calibrateZero();        // 135 0x87 Calibrates "Zero" (Note: Zero refers to 400ppm for this sensor)
      //  delay(100);
    }
    if (messageTemp == "Recover") {
      myMHZ19.recoveryReset();        // 120  0 Recovery Reset Changes operation mode and performs MCU reset
      //  delay(100);
    }
    if (messageTemp == "getABC") {
      int CO2 =  myMHZ19.getABC();  // 125 0x7D Returns ABC logic status (1 - enabled, 0 - disabled)
      MQTT_ATT["15.AutoCal."] = CO2;
      serializeJson(MQTT_ATT, mqtt_msg);
      FHEM_Client.publish(mqtt_topic, mqtt_msg);
    }
    doc.clear();
  }
}

void fhemconnect() {
  while (!FHEM_Client.connected()) {                 // Loop until reconnected
    Serial.print("FHEM_MQTT connecting ...");
    String MQclientId = "Z19B_01";                   // client ID
    if (FHEM_Client.connect(MQclientId.c_str())) {   // connect now
      Serial.println("connected");
      FHEM_Client.subscribe("/ESP32/7D80806A/+");
      //      FHEM_Client.subscribe("/ESP32/7D80806A/cmd");
    } else {
      Serial.print("failed, status code =");
      Serial.print(FHEM_Client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);                               // Wait 5 seconds before retrying
    }
  }
}

void setRange(int range)
{
  Serial.println("Setting range..");
  myMHZ19.setRange(range);                        // request new range write
  if ((myMHZ19.errorCode == RESULT_OK) && (myMHZ19.getRange() == range))     //RESULT_OK is an alias from the library,
    Serial.println("Range successfully applied");
  else
  {
    Serial.print("Failed to set Range! Error Code: ");
    Serial.println(myMHZ19.errorCode);          // Get the Error Code value
  }
}

void verifyRange(int range)
{
  Serial.println("Requesting new range.");
  myMHZ19.setRange(range);                       // request new range write
  if (myMHZ19.getRange() == range)               // Send command to device to return it's range value.
  {
    Serial.print("Range :");
    Serial.print(range);
    Serial.println(" successfully applied.");  // Success
  }   else
    Serial.println("Failed to apply range.");  // Failed
}

void getCredentials()
{
  WiFiClient NVS_Client = NVS_Server.available();   // listen for incoming clients we name them NVS_Clien

  if (NVS_Client) {                          // if you get a NVS_Client,,
    Serial.println("Neuer Client");            // print a message out the serial port
    String currentLine = "";                 // make a String to hold incoming data from the client
    while (NVS_Client.connected()) {         // loop while the client's connected
      if (NVS_Client.available()) {          // if there's bytes to read from the client,
        char c = NVS_Client.read();          // read a byte, then
        Serial.write(c);                     // print it out the serial monitor
        if (c == '\n') {                     // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            NVS_Client.println("HTTP/1.1 200 OK");
            NVS_Client.println("Content-type:text/html");
            NVS_Client.println();

            // the content of the HTTP response follows the header:
            NVS_Client.print("<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>");
            NVS_Client.println();   // The HTTP response ends with another blank line:
            break;                  // break out of the while loop:
          } else {                  // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;        // add it to the end of the currentLine
          continue;
        }

        if (currentLine.startsWith("GET /a?ssid=") ) {
          //Expecting something like:    GET /a?ssid=blahhhh&pass=poooo

          String qsid, qpass;
          qsid = urlDecode(currentLine.substring(12, currentLine.indexOf('&'))); //parse ssid
          qpass = urlDecode(currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' '))); //parse password

          Serial.printf("Writing new Password to NVS %s\n", "xxxx");   // Serial.println(qpass);
          Serial.printf("Writing new ssid to NVS %s\n", qsid);         // Serial.println(qpass);

          preferences.begin("wifi", false);      // Note: Namespace name is limited to 15 chars
          preferences.clear();                   // Remove all preferences under opened namespace
          preferences.putString("ssid", qsid);
          preferences.putString("password", qpass);
          delay(300);
          preferences.end();

          NVS_Client.println("HTTP/1.1 200 OK");
          NVS_Client.println("Content-type:text/html");
          NVS_Client.println();
          // the content of the HTTP response follows the header:
          NVS_Client.print("<h1>OK! Restarting in 5 seconds...</h1>");
          NVS_Client.println();
          Serial.println("Restarting in 5 seconds...");
          delay(5000);
          ESP.restart();
        }
      }
    }
    NVS_Client.stop();                  // close the connection:
    Serial.println("NVS_client disconnected");
  }
}

// Setze Zeit nach NTP, zur x.509 überprüfung und Tagesberechnung für Update
void Check_Last_OTA() {
  configTime(3600, 3600, "pool.ntp.org");  // middle Europa
  //  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC
  Serial.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    yield();
    delay(500);
    Serial.print(F("."));
    now = time(nullptr);
  }
  Serial.println(F(""));
  //  gmtime_r(&now, &timeinfo);
  getLocalTime(&timeinfo);
  Serial.print(F("Aktuelle Zeit: "));
  Serial.print(asctime(&timeinfo));
  Serial.print(F("Aktueller Tag: "));
  Serial.println(timeinfo.tm_yday);

  if (OTA_day == timeinfo.tm_yday) {
    OTA_checked = true;
    Serial.println("Heute wurde bereits auf Updates geprüft !");
  }
  else
  {
    preferences.begin("wifi", false);           // Note: Namespace name is limited to 15 chars
    Serial.println("Speichere aktuellen Tag in NVS");
    preferences.putUInt("OTA", timeinfo.tm_yday);
    delay(300);
    preferences.end();
  }
}
