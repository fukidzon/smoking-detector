// Smoking detector with loggind to SD card
// snmoking detection algorithm (c) Jan Mazanec, jmazanec@gmail.com, 14.3.2019
// Arduino UNO; SD card shield with RTC; SDS011 PM detector; active piezo buzzer; LEDs

//rules for detecting of smoking
// 1. if PM2.5 is 5 higher than a minute ago - smoking detected, last good value is saved (average of 10 values 1 minute ago)
// 2. 1 minute after smoking alarm starts we check if the PM2.5 value was exceeded by at elast 10 - otherwise we declare it a false alarm and reinitialize the array with current values
// 3a. end of smoking alarm: after 15 minutes we stop the smoking status (we assume smoking doesn't last longer than 15min, just the air got naturally bad somehow)
// 3b. end of smoking alarm: when the PM2.5 value is less than 5 over last_good for 30 seconds (PM values array is reinitialized to current PM value) - if this occured after less then 1 minute, we declare false alarm


// 60 seconds long history array of measured values
#define ARRAY_MAX 60

// status LED pins
#define GREEN_LED 7 // always ON
#define YELLOW_LED 6 // higher PM values
#define RED_LED 5 // smoking detected
// active piezo buzzer - sound when LOW
#define BUZZER 3

// SDshield tutorial:
// https://navody.arduino-shop.cz/navody-k-produktum/data-logger-shield.html
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include <Time.h>

RTC_DS1307 DS1307;
// CS pin for SD card - fixed for this shield
const int sd_CS = 10;

#include <SDS011.h>
float p10,p25;
SDS011 my_sds;
int error;

time_t previous_time=0;
time_t smoke_begin_time;
byte smoking_status=0; // 1=smoking; 2=pm25 or pm10 over limit (25, 50)
unsigned int good_seconds; // how many secods the air has been OK

// history of measurements
int values_pm10[ARRAY_MAX];
int values_pm25[ARRAY_MAX];

//unsigned int p25_compare_alarm=0;
// unsigned int p10_compare_alarm=0;

int last_good_10;
int last_good_25;
byte previous_day;

// ARRAY manipulation functions
void add_to_array(int pole[ARRAY_MAX],  int hodnota) {
  for (int i=(ARRAY_MAX-1); i>0; i--) {
    pole[i]=pole[i-1];
  }
  pole[0]=hodnota;
}

void initialize_array( int pole[ARRAY_MAX],  int hodnota=0) {
  for (int i=0; i<ARRAY_MAX; i++) {
    pole[i]=hodnota;
  } 
}

void print_array( int pole[ARRAY_MAX]) {
  for (int i=0; i<ARRAY_MAX; i++) {
    Serial.print(pole[i]);
    Serial.print(" ");
  }
  Serial.println(); 
}

void setup() {
  Serial.begin(115200);
    
  // check if time clock is attached
  if (! DS1307.begin()) {
    Serial.println(F("Clock not found"));
    while (1);
  }
  // check if time clock is running
  if (! DS1307.isrunning()) {
    Serial.println(F("Clock not running! Starting..."));
  }
  // check of SD card
  if (!SD.begin(sd_CS)) {
    Serial.println(F("SD card not available or broken!"));
    return;
  }
  
  my_sds.begin(8,9);
  
  initialize_array(values_pm10,0);
  initialize_array(values_pm25,0);
  
  DateTime current_time = DS1307.now();
  previous_day = current_time.day();
  
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH); 
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(RED_LED, HIGH);
}

byte starting_seconds=61;
int alarm_count=0;
int false_alarm_count=0;
byte beep_count=0;
byte buzzer_status=0;
int max_p25=0;
 

void loop() {
  DateTime current_time = DS1307.now();
  time_t current_time_unix=current_time.unixtime();
  //if a second passed
  if (current_time_unix > previous_time) {
    previous_time = current_time_unix;
    if (previous_day != current_time.day()) {
      // zero the alarm counts on new day
      previous_day = current_time.day();
      alarm_count=0;
      false_alarm_count=0;
    }

    // buzzer beeping
    if (beep_count>0) {
      //change status from beep to silence
      if (buzzer_status==0) {
        buzzer_status=1;
        digitalWrite(BUZZER, LOW); 
      } else {
        buzzer_status=0;
        digitalWrite(BUZZER, HIGH);
        beep_count--; 
      }
    }
    if (beep_count==0) {
      buzzer_status=0;
      digitalWrite(BUZZER, HIGH);
    } 
    // read SDS011 - PM values
    error = my_sds.read(&p25,&p10);
    if (! error) {
      
      add_to_array(values_pm10,round(p10));
      add_to_array(values_pm25,round(p25));
      char filename [13];
      // filename in format YEARMMDD
      sprintf (filename, "%04u%02u%02u.csv", current_time.year(), current_time.month(), current_time.day());
      // bytes needed: 7-cas ; 5-p25 ; 5 p10 ; 1-status ; message (15sOK,10min,false,smoke) ; 3 pocet alarmov; 3 pocet false = 36
      char dataString [36];
      char message [6];
      sprintf(message,"OK");
       // first 60 seconds we just measure and save values
      if (starting_seconds==0) {
        //print_array(values_pm25);
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(RED_LED, LOW);
        // turn on yellow led when the PM values exceed the norm for good air quality
        if (values_pm25[0]>25 || values_pm10[0]>50) {
          if (smoking_status != 1) { smoking_status=2;}
          digitalWrite(YELLOW_LED, HIGH);
          sprintf(message,"high");
        }
        // reset status
        if (values_pm25[0]<=25 && values_pm10[0]<=50 && smoking_status!=1) {smoking_status=0;}
        
        //smoking detected
        if (((values_pm25[0]-values_pm25[ARRAY_MAX-1])>5) && (smoking_status != 1)) {
          max_p25=0;
          // buzzer beeps
          beep_count=5;
          alarm_count++;
          smoking_status = 1;
          digitalWrite(RED_LED, HIGH);
          smoke_begin_time=current_time_unix;
          // save average of last good PM values 1 minute ago
          last_good_10=0;
          last_good_25=0;
          for (int i=0; i<10; i++) {
            last_good_10+=values_pm10[ARRAY_MAX-1-i];
            last_good_25+=values_pm25[ARRAY_MAX-1-i];
          }
          last_good_10=round(last_good_10 / 10.0);
          last_good_25=round(last_good_25 / 10.0);          
        }
        if (smoking_status == 1) {
          if ((values_pm25[0]-last_good_25)>max_p25) {
            max_p25 = values_pm25[0]-last_good_25; 
          }
          sprintf(message,"SMOKE");
          digitalWrite(RED_LED, HIGH);
          // after 15minutes we stop the smoking status - assumption is that the air just got bad and that smoking doesn't last longer than 15min
          if ((current_time_unix-smoke_begin_time)>900) {
             smoking_status = 0;
             // message+="po 15 min";
             sprintf(message,"15min");
             initialize_array(values_pm10,round(p10));
             initialize_array(values_pm25,round(p25));
             good_seconds=0;
             // one beep at the end of smoking alarm
             beep_count=1;
          }
          // if values are less than 6 higher than the status before smoking - count seconds
          if ((values_pm25[0]-last_good_25)<6) {
            good_seconds++;
          }
          // after 30seconds of good air, we end the smoking alarm
          if (good_seconds>30) {
             smoking_status = 0;
             // message+="15x good";
             sprintf(message,"30sOK");
             // array of PM values are initialized with current PM value
             initialize_array(values_pm10,round(p10));
             initialize_array(values_pm25,round(p25));
             good_seconds=0;
             //if smoking alarm lasted less than 1 minut and the PM25 value was not higher than +10, declare it a false alarm
             // one beep at the end of smoking alarm
             beep_count=1;
             if ((current_time_unix-smoke_begin_time)<=60 && (max_p25<10)) {
               false_alarm_count++;
               // two beeps at the end of false smoking alarm
               beep_count=2;
             }
             
          }
          // at one minute after alarm start we check the values and if they didn't exceed +10, we declare a false alarm
          if ((current_time_unix-smoke_begin_time)==60 ) {
            if (max_p25<10) { 
              smoking_status = 0;
              //message+="false alarm";
              sprintf(message,"false");
              false_alarm_count++;
              initialize_array(values_pm10,round(p10));
              initialize_array(values_pm25,round(p25));
              good_seconds=0;
              // two beeps at the end of false smoking alarm
              beep_count=2;
 
            }
          }
          
        }
      } else { 
        starting_seconds--;
      }
      // CSV format: time; pm2.5 value; pm10 value; smoking status; total alarms today; false alarms today; status message
      sprintf (dataString, "%02u:%02u:%02u;%d.%d;%d.%d;%d;%d;%d;%s", current_time.hour(), current_time.minute(), current_time.second(),(int)p25,(int)(p25*10)%10,(int)p10,(int)(p10*10)%10,smoking_status,alarm_count,false_alarm_count,message);
      Serial.println(dataString);
 
      File dataWrite = SD.open(filename, FILE_WRITE);
      //Serial.println(dataString);
      if (dataWrite) {
        dataWrite.println(dataString);
        dataWrite.close();
        Serial.println(F("Write on SD successfull"));
      }
      // when problem writing
      else {
        Serial.print(F("Error opening file "));
        Serial.println(filename);
      }
    }
  } else {
    // waiting for check for next full second
    delay(100);
  }
}
