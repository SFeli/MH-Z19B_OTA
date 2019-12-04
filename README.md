# MH-Z19B_OTA
ESP32 with CO2-Sensor Z19B with OTA-Update
- first check if WiFi is aviable - without no progress
- second check if credentials are storred in non-volatile storage (NVS)
  - if not create an access point and get the User and PW -> store them in NVS
- third open Wifi and connect to MQTT - Brocker
- forth send Headerinformations 
- start loop 
  - with readings to MQTT-Broker
- end loop
