# Open-Intelligence-Arduino-Client

<p align="start">
  <img src="https://github.com/norkator/Open-Intelligence-Arduino-Client/tree/master/MegaDisplay/page1.png" alt="page">
</p>

Small Arduino information display unit for Open Intelligence project. 
Currently showing total detection count for start of `current day` and latest license plate reading using 
LiquidCrystal lcd module.
Later maybe can post some button push events and get more information.

This program will also utilize http port 80 to host small web page (first readme image), I found neat tutorial
for base html arduino relay control and modified/styled it to show also two DHT22 reading on table structure.
I will use relay to control "tunnel" air blower to remove warm air fast from server room. Two DHT22 will be placed
on ceiling and other on floor to see room height temperature difference. 


###### Hardware requirements
* Arduino. I use `Mega 2560`
* Ethernet module like `Wiznet`
* Buzzer *(optional)*


###### Configuration
1. Change server address at script variable.
2. Write firmware to your Arduino.


## Authors

* **Norkator** - *Initial work* - [norkator](https://github.com/norkator)
