# ESP32 LED & MAINS Applicance controller

Implements LED Lamp with PWM brightness control and MAINS application ON/OFF using HTTP GET requests. 
Also, ability to set scheduled ON/OFF for both

LED (12v strip) is PWM controlled, MAINS appliances can be controlled using a solid state relay or mechanical relay

The project was later used to automate water tank pump operation at the house using sense wires to detect once the tank is full and the motor is turned on using the schedule on function 3 times per day.

### Following are the HTTP URLs for different commands (replace with the correct IP address)

Get ESP details (Boot time, scheduled ON/OFF times)
http://192.168.0.110/getdevicetime

Increase LED brightness
http://192.168.0.110/setledbrightness?inc

Decrease LED brightness
http://192.168.0.110/setledbrightness?dec

Set LED brightness to medium
http://192.168.0.110/setledbrightness?med

Turn off LED
http://192.168.0.110/setledbrightness?min

Turn on LED (Max brightness)
http://192.168.0.110/setledbrightness?max

Set scheduled OFF for 12:30 AM 
http://192.168.0.110/setalarm?hours=00&mins=30&type=off

Set scheduled ON for 4:45 PM 
http://192.168.0.110/setalarm?hours=16&mins=45&type=on

Delete all schedules
http://192.168.0.110/deletealarms


