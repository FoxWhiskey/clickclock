#include <Arduino.h>
#include <wificonfig.h>
#include <ESP_EEPROM.h>
#include <log.h>
#include <TimeLib.h>
#include <libClockWork.h>
#include <libsavestate.h>

extern byte mac[2];
extern loglevel setloglevel;
extern int8 timeZone;
extern float drift;

/**
 * @brief standard constructor, will load default values into systemdata object, 
 * @brief initialize EEPROM-memory and load stored data into object (if available)
 * @brief default values are taken from `wificonfig.h`
*/
systemdata::systemdata() :  _hour(HOUR_HAND_DEFAULT),
                            _minute(MIN_HAND_DEFAULT),
                            _ISRcom(0),
                            _timezone(TIMEZONE),
                            _setloglevel(DEFAULTLEVEL),
                            _ssid(SSID),
                            _ntpinterval(NTPINTERVAL),
                            _pass(PASSWD),
                            _hostname("clickclock"),
                            _ntpserver(NTPSERVER),
                            _drift(0.0)
{
    
    EEPROM.begin(sizeof(*this));

    if (EEPROM.percentUsed() >= 0) {
        EEPROM.get(0,*this);
    } else EEPROM.put(0,*this);
};

/**
 * @brief return the percentage of a used EEPROM memory page. 100% will result in a page erase.
 * @brief see documentation of ESP_EEPROM
*/
int systemdata::percentUsed() { return EEPROM.percentUsed();}

/**
 * @brief writes system state properties into EEPROM memory
 * @return true, if successful
*/
bool systemdata::write() {return EEPROM.commit();}

/**
 * @brief copies cstring data from the system state object to a passed char-pointer
 * @param get_service type of string data, either of HOSTNAME, HOSTNAME_MAC, SSId, PASS or NTPSRVR
 * @param char*& a reference to an allocated cstring of sufficient size, where data will be copied to (HOSTNAME,HOSTNAME_MAC). Or a pointer to char (SSId,PASS,NTPSVR)
 * @return int currently always 0
*/
int systemdata::get(get_service get_type,char* &data) {

    EEPROM.get(0,*this);
    switch (get_type)
    {
    case HOSTNAME :
        strcpy(data,_hostname);
        break;
    case HOSTNAME_MAC:
        strcpy(data,_hostname);
        if (strlen(data)+4 <= MAX_HOSTNAME_S) snprintf(data+strlen(data),MAX_HOSTNAME_S,"-%02x%02x",mac[0],mac[1]);
        else  snprintf(data+(MAX_HOSTNAME_S - 4),MAX_HOSTNAME_S,"-%02X%02X",mac[0],mac[1]);
        break;
    case SSId:
        data = _ssid;
        break;
    case PASS:
        data = _pass;
        break;
    case NTPSVR:
        data = _ntpserver;
        break;
    default:
        data = NULL;
        break;
    }
return 0;    
};

/**
 * @brief copies system data to a reference of uint_8
 * @param get_service type of data, either of HANDHOUR or HANDMINUTE
*/
uint8_t systemdata::get_hand(get_service get_type) {

    switch (get_type)
    {
    case HANDHOUR:
        return _hour;
        break;
    case HANDMINUTE:
        return _minute;
        break;
    default:
        break;
    }
return -1;
};

/**
 * @brief return ISRcom flags from EEPROM and set ISRcom accordingly
*/
void systemdata::get_flags() {

    if (_ISRcom & F_POLARITY) ISRcom |= F_POLARITY;
    else ISRcom &= ~F_POLARITY;
    if (_ISRcom & F_CM_SET) ISRcom |= F_CM_SET;
    else ISRcom &= ~F_CM_SET;
    
}

/**
 * @brief returns the virtual millisecond (in µs) for this system, which is the "base" for the
 * @brief interval time of Timer1 interrupt service routine. If '_drift' is positive (negative),
 * @brief the clock is late and the interrupt frequency must be set higher (lower). 
*/
void systemdata::get(float& data) {
    
    data = _drift;
}

/**
 * @brief copies saved loglevel to a reference of loglevel
*/
void systemdata::get(loglevel& data) {

    data = _setloglevel;

}

/**
 * @brief copies saved NTP resync-interval to a reference of time_t
*/
void systemdata::get(time_t &data) {
  
    data = _ntpinterval;
}

/**
 * @brief copies saved time zone to a reference of int
*/
void systemdata::get(int8 &data) {
  
    data = _timezone;
}

/**
 * @brief log status info of systemstate object
*/
void systemdata::status() {

    if (EEPROM.percentUsed() < 0) log(WARN,__FUNCTION__,"EEPROM: systemstate has not been stored yet.");
    else log(INFO,__FUNCTION__,"EEPROM: systemstate is of size %iB. %i%% EEPROM memory used.",sizeof(*this),EEPROM.percentUsed());
}


/**
 * @brief collect relevant systemdata, update object and put it into EEPROM-buffer. DO NOT WRTIE!
*/
void systemdata::collect(){

    _hour = hour(tt_hands);
    _minute = minute(tt_hands);
    _ISRcom = ISRcom;
    _setloglevel = setloglevel;
    _timezone = timeZone;
    _drift = drift;

    EEPROM.put(0,*this);
}


