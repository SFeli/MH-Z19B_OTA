# MH-Z19B_OTA
ESP32 with CO2-Sensor Z19B with OTA-Update
First check if WiFi is aviable - without no progress
second check if credentials are storred in non-volatile storage (NVS)
      if not create an access point and get the User and PW -> store them in NVS
open Wifi and connect to MQTT - Brocker
send Headerinformations 
loop 
  with readings to MQTT-Broker
end loop
