
# Welcome to honeypal!

Hi! Let's take care of our bees.


# Getting Started

## Local development Setup
### Server

    git clone https://github.com/fabriciopk/hive-status.git && cd hive-status/server
    docker-compose -d up

### Node (ESP-32, WI-FI)

    idf.py -p  <ESP_32_PORT> clean  flash
  
## Wi-Fi Node Parts List

 - ESP-32 - microcontroller and communication module
 - DHT11 - external humidity and temperature sensor
 - HX11 - weight sensor
 - DS18x20 - internal temperature sensor

### Build instructions


