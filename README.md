# Servet IV - LanuzaSat
The [Servet project](https://servet.ibercivis.es/) allows people to create their own capsules of different weights which rise with helium balloons to around 35 km.

This repository contains the data extracted from the LanuzaSat capsule, which participated in the 400 gram category. It collected several different types of data. These are:
* Temperature (inside and outside of the capsule)
* Pressure
* Humidity
* GPS (latitude, longitude, altitude and time)
* Radioactivity (measured by an SBM-20 Geiger tube)

It also contains the three different pieces of code that made it all work
* Servet_LoRa_Capsule_V6.2.ino (which reads all sensor data, sends it through LoRa and cellphone data plan, and stores it locally)
* Servet_LoRa_Receiver_6.2.ino (which is loaded in a base station that saves what it receives to an SD card)
* Servet_LoRaWAN_GPS_1.1 (which sends the GPS coordinates of the capsule through LoRaWAN)
