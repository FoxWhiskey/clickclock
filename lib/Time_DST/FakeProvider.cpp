#include<Arduino.h>
#include<TimeLib.h>
#include<FakeProvider.h>

const TimeElements Ftm = (TimeElements){.Second = FSECOND, .Minute = FMINUTE, .Hour = FHOUR, .Wday = 0, .Day = FDAY, .Month = FMONTH, .Year = FYEAR -1970};
/**
 * @brief Simulate a UTC time source, the start time can be set arbitrarily in the header file
 * @return a time_t object representing UTC-time 
*/
time_t getFakeTime() {

static time_t faketime = makeTime(Ftm);
static unsigned long prevMillis = millis();
faketime += (millis()-prevMillis)/1000;

prevMillis = millis();

return faketime;
}