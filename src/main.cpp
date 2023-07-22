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

#define HOUR_HAND_DEFAULT 6       // the default position of hour hand
#define  MIN_HAND_DEFAULT 23       // the default position of minute hand

extern const char ssid[];
extern const char pass[];
extern const char ntpServerName[];
extern const int timeZone;
extern WiFiUDP Udp;
extern unsigned int localPort;
time_t NTPsyncInterval = 1800;
TimeElements hand;
time_t systemstart = 0;
extern time_t tt_hands;

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
  } 

  systemstart = now();                  // store the first call to now() as system start
  digitalClockDisplay();
  breakTime(now(),hand);                // load current date into "hand"
  hand.Minute = MIN_HAND_DEFAULT;       // set clockwork minute
  hand.Second = 0;                      // set clockwork second  // bugfix issue #13
  tt_hands = makeTime(hand);            // derive time_t object from "hand"  // bugfix issue #9
  transformDHP(tt_hands,HOUR_HAND_DEFAULT); // modify "tt_hands" to represent current clockwork setting // bugfix issue #9
  hand.Hour = hour2clockface(hour(tt_hands)); // then update clockwork hour
  // ********** timer interrupt setup **********
  setupInterrupts();
  // ********** synchronise clock work to system time and run clock
  ISRcom |= F_SEC00;                // this can ONLY be done during setup, as it immediately follows setupInterrupts() !!!
  syncClockWork();
  log(DEBUG,__FUNCTION__,"ISRcom: %i",int(ISRcom));
  log(INFO, __FUNCTION__, "%s", "clickclock-setup completed.");
  
}

time_t prevDisplay = 0;
time_t time_now = 0;
void loop()
{
  logISR();
  delay(200);
  if (ISRcom & F_SEC00) {
     if (timeStatus() != timeNotSet)
     {
       time_now = now();
       if (time_now != prevDisplay)
       { // update the display only if time has changed
         prevDisplay = time_now;
         log(INFO,__FUNCTION__,"[%02i:%02i]: Hands at %02i:%02i",hour(prevDisplay),minute(prevDisplay),hour2clockface(hour(tt_hands)),minute(tt_hands));
         log(DEBUG,__FUNCTION__,"delta_NTP is %is",time_now-tt_hands);
         /*
          * Test on time gap between systemtime/NTP and clockwork time - resync clockwork if needed
          */
        if (abs(prevDisplay-tt_hands) > MAX_NTP_DEVIATION)   // if NTP and clockwork out of sync
          {
            log(WARN,__FUNCTION__,"delta_NTP is %is Clockwork out of sync! Resyncing...",abs(prevDisplay-tt_hands));
            ISRcom &= ~F_MINUTE_EN;              // stop clockwork
          }
       }
     } else log(WARN,__FUNCTION__,"timeNotSet");
  }

  // check if compensation minute has been requested
  if (ISRbtn & F_BUTN1LONG) CompensateMinute();
  if ((ISRcom & F_CM_SET) && !(ISRcom & F_FSTFWD_EN)) ISRcom |= F_MINUTE_EN;
}