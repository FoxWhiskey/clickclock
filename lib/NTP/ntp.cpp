#define FOXLOGGER
#include<Arduino.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <wificonfig.h>
#ifdef FOXLOGGER
#include "log.h"
#endif
#include "ntp.h"

char* ntpServerName = NULL;                       // will be set by systemdata class 
WiFiUDP Udp;
unsigned int localPort = 8888;                    // local port to listen for UDP packets

/**
 * @brief prints date and time to serial console
 * @param function function pointer that returns a `time_t` element to be printed to console
*/
void digitalClockDisplay(time_t (*PtimeSrc)())
{
  time_t nowtime = PtimeSrc();
  // digital clock display of the time
  #ifndef FOXLOGGER
  Serial.print(hour(nowtime));
  printDigits(minute(nowtime));
  printDigits(second(nowtime));
  Serial.print(" ");
  Serial.print(day(nowtime));
  Serial.print(".");
  Serial.print(month(nowtime));
  Serial.print(".");
  Serial.print(year(nowtime));
  Serial.println();
  #else
  log(INFO,__FUNCTION__,"[%02i.%02i.%i] %02i:%02i:%02i %s",day(nowtime),month(nowtime),year(nowtime),hour(nowtime),minute(nowtime),second(nowtime), dst() == true ? "DST" : "");
  #endif
}
#ifndef FOXLOGGER
void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
#endif

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  #ifndef FOXLOGGER
  Serial.println("Transmit NTP Request");
  #else
  log(DEBUG,__FUNCTION__,"Transmit NTP Request");
  #endif
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  #ifndef FOXLOGGER
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  #else
  log(DEBUG,__FUNCTION__,"%s (%s)",ntpServerName,ntpServerIP.toString().c_str());
  #endif
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      #ifndef FOXLOGGER
      Serial.println("Receive NTP Response");
      #else
      log(DEBUG,__FUNCTION__,"Receive NTP Response");
      #endif
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      log(INFO,__FUNCTION__,"System clock updated.");
      return secsSince1900 - 2208988800UL;
    }
  }
  #ifndef FOXLOGGER
  Serial.println("No NTP Response :-(");
  #else
  log(WARN,__FUNCTION__,"No NTP Response :-(");
  #endif
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}