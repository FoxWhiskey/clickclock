#include <Arduino.h>
#include <log.h>
#include <welcome.h>
#include <TimeLib.h>
#include <libClockWork.h>
#include <ESP8266_ISR_Timer.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "ntp.h"

loglevel setloglevel = DEBUG;

#define HOUR_HAND_DEFAULT 19        // the default position of hour hand
#define  MIN_HAND_DEFAULT 05        // the default position of minute hand

extern const char ssid[];
extern const char pass[];
extern const char ntpServerName[];
extern const int timeZone;
extern WiFiUDP Udp;
extern unsigned int localPort;
time_t NTPsyncInterval = 1800;
TimeElements hand;

void setup()
{
  Serial.begin(115200);
  welcome();
  // ******** set outputs *********************
  pinMode(PIN_D1,OUTPUT);
  pinMode(PIN_D2,OUTPUT);
  pinMode(PIN_LED,OUTPUT);
  digitalWrite(PIN_D1,HIGH);
  digitalWrite(PIN_D2,HIGH);
  digitalWrite(PIN_D5,HIGH);
  digitalWrite(PIN_LED,HIGH);
  // ******** WiFi Setup ********************** 
  WiFi.hostname("clickclock");
  WiFi.begin(ssid, pass);
  log(INFO, __FUNCTION__, "Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    log(DEBUG,__FUNCTION__,"Waiting...");
  };
  log(INFO, __FUNCTION__, "Connect! IP-Address is %s", WiFi.localIP().toString().c_str());

  // ********** NTP & time setup **************
  Udp.begin(localPort);
  log(INFO, __FUNCTION__, "Local port: %i", Udp.localPort());
  log(INFO,__FUNCTION__,"Setting DST zone");
  setDSTProvider(CET);
  log(INFO, __FUNCTION__, "Waiting for sync...");
  setSyncProvider(getNtpTime);
  setSyncInterval(NTPsyncInterval);
  log(INFO,__FUNCTION__,"NTP-interval is %i sec",(uint32_t)NTPsyncInterval);
  while (timeStatus() == timeNotSet) {
    delay(100);
    breakTime(now(),hand);
  }
  digitalClockDisplay();
  // ********** timer interrupt setup **********
  setupInterrupts();
  // ********** synchronise clock work to system time and run clock
  hand.Hour = HOUR_HAND_DEFAULT;    // the present time display before sync'ing...
  hand.Minute = MIN_HAND_DEFAULT;
  syncClockWork();
  log(DEBUG,__FUNCTION__,"ISRcom: %i",int(ISRcom));
  log(INFO, __FUNCTION__, "%s", "Setup completed");
  
}

time_t prevDisplay = 0;
void loop()
{
  logISRcom();

  delay(200);
  if (ISRcom & F_SEC00) {
     if (timeStatus() != timeNotSet)
     {
       if (now() != prevDisplay)
       { // update the display only if time has changed
         prevDisplay = now();
         digitalClockDisplay();
       }
     }
  }
  
}