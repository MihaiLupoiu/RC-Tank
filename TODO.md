# TODO

ESP32 Tank build with IDF 4.2 

## Basic
- [x] Working Motor using DRV8833
- [X] Wifi Connection
- [X] MQTT Connection
- [X] Contorller module connected to MQTT Client for sending movement instructions
- [X] Test all components connected.

## Nice To Have

- [ ] Modem Connection with 2/3G with LTE
- [ ] Battery Monitor
- [ ] Solar Panels for Chargin Battery
- [ ] Store data locally
- [ ] Console to interact with internal Pub/Sub
- [ ] MQTT Device identificaiton with every message.
- [ ] MQTT QoS 
- [ ] MQTT Persistency of messages
- [ ] MQTT User/Password
- [ ] MQTT SSL Certificate
- [ ] MQTT Filter of topics
- [ ] MQTT Automatic Subscription
- [ ] Module basic structure. name, status, autostart, retray  
- [ ] System Configuration Module for storing and loading internal configuraiton
- [ ] Configuration Managment from exterior
- [ ] Know when it is online and offline. Periodic ping to a server.

## Improvements 
- [ ] Improve module creation, more automatic. Certer of truth for the status of them.
- [ ] Improve module name usage internally (Macros or header file available for everyone)
- [ ] Time reaction to changes in motor driver.
- [ ] Unit Testing in ESP32 (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
- [ ] 
