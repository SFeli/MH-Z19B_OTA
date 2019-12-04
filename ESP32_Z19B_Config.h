/* Configuration - File mit Z19B CO2 - Daten auszulesen
  */
#include <Arduino.h>

#define Programname "ESP32_Z19B_09"
#define RX_PIN 21               // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 22               // Tx pin which the MHZ19 Rx pin is attached to
#define AP_SSID "Z19B_D32"      // can set ap hostname here for Access Point
#define STA_SSID "Z19B_D32"     // can set hostname here for notmal Station WIFI
#define BAUDRATE 9600           // Native to the sensor (do not change)

// --- intern Variables ---
static volatile bool wifi_connected = false;
static volatile bool OTA_checked = false;
String wifiSSID, wifiPassword;
unsigned int OTA_day;
struct tm timeinfo;

// --- Header & Sensor Information --- //
char ChipID[10];                // ChipID from ESP32
char myVersion[4];              // needed for MH-Z19B
String SensVersion;
int frequenz = 500;             // Frequenz der Übertragung (min 5 Sekunden, da nur alle 5 Sekunden gemessen wird.)
				// 500 -> ca. 8 Minuten.
unsigned long getDataTimer = 0; // Variable to store timer interval
bool sens_autoCalibratation = false;
int sens_Range = 2000;

/* MQTT - Topics */
char mqtt_topic[40];            // like /ESP32/A0010202/  where A0010202 is derived from the ChipID and unique
char mqtt_msg[220];
const char* mqtt_server = "192.168.0.200";  // local MQTT-Borker for additional monitoring

const char* serverOTA = "www.felmayer.de";  // Server URL for OTA-File (.bin)
const char* rootCACertificate = \
                                "-----BEGIN CERTIFICATE-----\n" \
                                "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
                                "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
                                "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
                                "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
                                "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
                                "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
                                "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
                                "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
                                "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
                                "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
                                "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
                                "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
                                "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
                                "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
                                "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
                                "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
                                "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
                                "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
                                "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
                                "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
                                "-----END CERTIFICATE-----\n";