#include <Arduino.h>
#include <log.h>
#include <welcome.h>
#include <TimeLib.h>
#include <libClockWork.h>
#include <ESP8266_ISR_Timer.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "ntp.h"

loglevel setloglevel = INFO;

#define HOUR_HAND_DEFAULT 21       // the default position of hour hand
#define  MIN_HAND_DEFAULT 58       // the default position of minute hand

extern const char ssid[];
extern const char pass[];
extern const char ntpServerName[];
extern const int timeZone;
extern WiFiUDP Udp;
extern unsigned int localPort;
time_t NTPsyncInterval = 1800;
TimeElements hand;
extern time_t tt_hands;
extern boolean minute_set;

void setup()
{
  Serial.begin(115200);
  welcome();
  // ******** set outputs *********************
  pinMode(PIN_D1,OUTPUT);
  pinMode(PIN_D2,OUTPUT);
  pinMode(PIN_D5,OUTPUT);
  pinMode(PIN_LED,OUTPUT);
  pinMode(PIN_D6,INPUT_PULLUP);
  pinMode(PIN_D7,INPUT_PULLUP);

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
  ISRcom |= F_SEC00;                // this can ONLY be done during setup, as it immediately follows setupInterrupts() !!!
  syncClockWork();
  log(DEBUG,__FUNCTION__,"ISRcom: %i",int(ISRcom));
  log(INFO, __FUNCTION__, "%s", "clickclock-setup completed.");
  
}

time_t prevDisplay = 0;
void loop()
{
  logISR();
  delay(200);
  if (ISRcom & F_SEC00) {
     if (timeStatus() != timeNotSet)
     {
       if (now() != prevDisplay)
       { // update the display only if time has changed
         prevDisplay = now();
         log(INFO,__FUNCTION__,"[%02i:%02i]: Hands at %02i:%02i",hour(prevDisplay),minute(prevDisplay),hour(tt_hands),minute(tt_hands));
       }
     }
  }
  // check if compensation minute has been requested
  if (ISRbtn & F_BUTN1LONG) CompensateMinute();
  if (minute_set & !(ISRcom & F_FSTFWD_EN)) ISRcom |= F_MINUTE_EN;
}