# Detector of cigarette smoke

Our neighbours smoke at the balcony so I decided to build a smoking detector based on PM values (only reasonable property of air I was able measure for this purpose). 
Current prototype is based on Arduino board (tests are mare with Arduino UNO + SDshield with RTC) and SDS011 PM detector (+LEDs and piezo buzzer)

![First prototype](img/SDshield_prototype.jpg)

## rules for detecting of smoking
* if PM2.5 is 5 higher than a minute ago - smoking detected, last good value is saved (average of 10 values 1 minute ago)
* one minute after smoking alarm starts we check if the PM2.5 value was exceeded by at elast 10 - otherwise we declare it a false alarm and reinitialize the array with current values
* end of smoking alarm: after 15 minutes we stop the smoking status (we assume smoking doesn't last longer than 15min, just the air got naturally bad somehow)
* end of smoking alarm: when the PM2.5 value is less than 5 over last_good for 30 seconds (PM values array is reinitialized to current PM value) - if this occured after less then 1 minute, we declare false alarm
 
## Further work
* improve the algorithm
* add communication (nrf, wifi with esp8266) and response to alarm
* try PMS5003 PM detector
 
For further investigation and algorithm improvemets: [Logged data](data)


