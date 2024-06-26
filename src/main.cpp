#include <Arduino.h>
#include <log.h>
#include <welcome.h>
#include <TimeLib.h>
#include <libsavestate.h>
#include <libClockWork.h>
#include <ESP8266_ISR_Timer.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FakeProvider.h>
#include "ntp.h"

loglevel setloglevel;                   // stores the loglevel of serial console. Loglevel can be set in 'wificonfig.h'
time_t NTPsyncInterval;                 // stores the NTP-resync interval. Default value can be set in 'wificonfig.h'
int8 timeZone;                          // stores the local time zone. Default value can be set in 'wificonfig.h'

extern ESP8266_ISR_Timer ISR_Timer;
extern char* ntpServerName;
extern WiFiUDP Udp;
extern unsigned int localPort;
extern uint16 virt_ms;
extern time_t mil_dif;
TimeElements hand;
unsigned long systemstart_millis = 0;
time_t systemstart_date = 0;
time_t delta_t = 0;
byte* macptr;
char* pStr = NULL;                      // a generic pointer to a char or an array of char
char* pPass= NULL;                      // a second generic pointer to a char or an array of char
char hostname[MAX_HOSTNAME_S]="";       // char array (cstring) to store hostname
byte  mac[2] = {0,0};                   // array to store least two bytes of MAC-address
float drift;                            // adaptive drift;
systemdata systemstate;       

void setup()
{
  // ******** restore system data from EEPROM ********
  systemstate.get(setloglevel);                // restore loglevel
  systemstate.get(NTPsyncInterval);            // restore NTP-interval
  systemstate.get(timeZone);                   // restore time zone
  systemstate.get_flags();                     // restore F_POLARITY and F_CM_SET of ISRcom
  systemstate.get(NTPSVR,ntpServerName);       // restore NTPSERVER 
  systemstate.get(drift);                      // restore virtual millisecond to compensate for oscillator drift
  // ******** Start serial console  ******************
  Serial.begin(115200);
  welcome();
  log(DEBUG,__FUNCTION__,"Size of systemdata-class: %i",sizeof(systemdata));
  systemstate.status();
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
  macptr = new byte[WL_MAC_ADDR_LENGTH];                // dynamically allocate memory to get last 2 bytes of MAC-address
  WiFi.macAddress(macptr);                              // get MAC-address
  mac[0] = macptr[4];                                   // copy last two bytes
  mac[1] = macptr[5];
  delete[] macptr;                                      // free memory
  pStr = hostname;
  systemstate.get(HOSTNAME_MAC,pStr);
  log(INFO,__FUNCTION__,"Hostname is %s",hostname);
  WiFi.hostname(hostname);
  systemstate.get(SSId,pStr);
  systemstate.get(PASS,pPass);
  WiFi.begin(pStr, pPass);
  log(INFO, __FUNCTION__, "Connecting to \'%s\', with hostname \'%s\'.", pStr, hostname);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    log(DEBUG,__FUNCTION__,"Waiting...");
  };
  log(DEBUG,__FUNCTION__,"MAC tail is %02x%02x",mac[0],mac[1]);
  log(INFO, __FUNCTION__, "Connect! IP-Address is %s", WiFi.localIP().toString().c_str());

  // ********** NTP & time setup **************
  Udp.begin(localPort);
  log(INFO, __FUNCTION__, "Local port: %i", Udp.localPort());
  setTimeZone(timeZone);
  log(INFO,__FUNCTION__,"Setting system time zone: %hhi",timeZone);
  log(INFO,__FUNCTION__,"Setting DST zone");
  setDSTProvider(CET);
  log(INFO, __FUNCTION__, "Waiting for sync...");
  setSyncProvider(getNtpTime);
  setSyncInterval(NTPsyncInterval);
  log(INFO,__FUNCTION__,"NTP-interval is %i sec",(uint32_t)NTPsyncInterval);
  while (timeStatus() == timeNotSet) {
    delay(100);
  } 

  digitalClockDisplay(nowdst);
  breakTime(nowdst(),hand);                           // load current date into "hand"
  hand.Minute = systemstate.get_hand(HANDMINUTE);  // set clockwork minute
  hand.Second = 0;                                 // set clockwork second  // bugfix issue #13
  tt_hands = makeTime(hand);                       // derive time_t object from "hand"  // bugfix issue #9
  transformDHP(systemstate.get_hand(HANDHOUR));    // modify "tt_hands" to represent current clockwork setting // bugfix issue #9
  hand.Hour = hour2clockface(hour(tt_hands));      // then update clockwork hour
  // ********** timer interrupt setup **********
  setupInterrupts(drift);
  // ********** remember time stamps for time drift calculations
  systemstart_date   = now();                  // system start  NTP date (UTC)
  systemstart_millis = millis();               // system start CPU clock date
  log(DEBUG,__FUNCTION__,"systemstart_millis is %lums",systemstart_millis);
  // ********** synchronise clock work to system time and run clock
  syncClockWork();
  log(DEBUG,__FUNCTION__,"ISRcom: %i",int(ISRcom));
  log(INFO, __FUNCTION__, "%s", "clickclock-setup completed.");
  
}

time_t time_NTP = 0;
time_t prevHands = 0;
time_t HandsNow = tt_hands;
void loop()
{
  //logISR(); //temporarily switched off
  delay(200);
  /*
  *  On normal operation, output status information every minute (on second 0)
  */
  if (ISRcom & F_SEC00) {
     if (timeStatus() != timeNotSet)
     {
       time_NTP = nowdst();
       if (HandsNow != prevHands)
       { // update the display only if clockwork position has changed
         prevHands = HandsNow;
         delta_t = time_NTP-tt_hands; 
         log(INFO,__FUNCTION__,"[%02i.%02i.%02i %02i:%02i:%02i %s]: Hands at %02i:%02i",day(time_NTP),month(time_NTP),year(time_NTP),hour(time_NTP),minute(time_NTP),second(time_NTP),dst(now()) == true ? "DST" :"",hour2clockface(hour(tt_hands)),minute(tt_hands));
         log(DEBUG,__FUNCTION__,"delta_NTP is %llds [%lld/%lld]",delta_t,time_NTP,tt_hands);
         
         /*
          * Test on time gap between systemtime/NTP and clockwork time - resync clockwork if needed
          */
        if ((abs(delta_t) > MAX_NTP_DEVIATION) && (ISRcom & F_MINUTE_EN))   // if NTP and clockwork out of sync
          {
            log(WARN,__FUNCTION__,"delta_NTP is %llds Clockwork out of sync! Resyncing...",time_NTP-tt_hands);
            ISRcom &= ~F_MINUTE_EN;                                         // stop clockwork
            if (abs(delta_t) > 59) {                                        // if deviation is 60sec and more (e.g. DST change, loss of NTP)
               if (delta_t < 0) tt_hands -= SECS_PER_DAY/2;                 // "pre"-correct tt_hands in case delta_t is negative: clockwork can only turn forward!!! Super bug!!
               breakTime(tt_hands,hand);                                    // and update "hand"
               log(DEBUG,__FUNCTION__,"tt_hands: %0d-%0d-%0d %0d:%0d:%0d",hand.Day,hand.Month,hand.Year+1970,hand.Hour,hand.Minute,hand.Second);
            //   if ((abs(delta_t) % 60) > 0) ISRcom &= ~F_SEC00;             // leave F_SEC00 set ONLY if no drift is present, then 
               syncClockWork();                                             // run a standard clockwork-sync
               log(DEBUG,__FUNCTION__,"tt_hands corrected to %lld",tt_hands);
            } else {
               ISRcom &= ~F_SEC00;                                              // else stop full minute indication
               drift = reSyncClockWork(delta_t,millis(),systemstart_millis);    // and handle time drift (deviation less than a minute)
               systemstate.collect();                                       // collect new drift value
               if (drift != -1234.5) systemstate.write();                   // and store it in EEPROM
            } 
          }
       }
     } else log(WARN,__FUNCTION__,"timeNotSet");
     HandsNow = tt_hands;
  }
  
  // check if compensation minute has been requested
  if (ISRbtn & F_BUTN1LONG) CompensateMinute();
  if ((ISRcom & F_CM_SET) && !(ISRcom & F_FSTFWD_EN)) ISRcom |= F_MINUTE_EN;

  // store system data in EEPROM and halt clockwork
  if (ISRbtn & F_BUTN2LONG && !(ISRcom & F_SAFE)) {
   
    systemstate.collect();
    systemstate.write();
    ISR_Timer.disableAll();
    ISRcom &= ~F_SEC00;
    ISRcom &= ~F_MINUTE_EN;

    ISRcom |= F_SAFE;
    
    systemstate.status();
    log(WARN,__FUNCTION__,"System state saved! CLOCKWORK HALTED! You may now power off!");
  }
}