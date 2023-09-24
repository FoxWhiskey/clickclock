/*  NOTE: 
   Sizes of c-string arrays and order of class members have been chosen carefully:
   To ensure a minimum amount of data written to the EEPROM, structural alignment has to be
   considered. As the largest data type is "time_t", all members must be aligned to 8 bytes.
   With the current setup, no padding bytes will be created and a total payload of 104 bytes
   can be used to store data on EEPROM.
   */
#define MAX_SSID_SIZE   24 
#define MAX_PW_SIZE     24
#define MAX_HOSTNAME_S  19

enum get_service {HOSTNAME,HOSTNAME_MAC,SSId,PASS,NTPSVR,HANDHOUR,HANDMINUTE,FLAG,DRIFT};

/**
 * @brief class to hold system data and to write it to system EEPRON.
 * @brief current size of object is 104 bytes ("loglevel" contributing 4)
*/
class systemdata
{
    public:
        systemdata();                           // constructor loads default data
        int percentUsed();
        void collect();
        void status();
        bool write();
        int get(get_service get_type,char* &data);  
        uint8_t get_hand(get_service get_type);
        void get_flags();
        void get(uint16_t& data);
        void get(loglevel& data);
        void get(time_t& data);
        void get(int8& data);
    private:
        uint8_t _hour;                           // current hour hand                              (1 byte)
        uint8_t _minute;                         // current minute hand                            (1 byte)
        uint8_t _ISRcom;                         // current state of the interrupt service routine communication register       (1 byte)
        int8 _timezone;                          // time zone of clock (1 for CET)                 (1 byte)
        loglevel _setloglevel;                   // current loglevel                               (4 bytes)
        char _ssid[MAX_SSID_SIZE];               // SSID (WiFi)                                    (24 bytes)
        time_t  _ntpinterval;                    // NTP resync interval of system clock (sec)      (8 bytes)
        char _pass[MAX_PW_SIZE];                 // password (WiFi)                                (24 bytes)
        char _hostname[MAX_HOSTNAME_S];          // hostname of clickclock board                   (19 bytes)
        char _ntpserver[MAX_HOSTNAME_S];         // hostname of NTPserver                          (19 bytes)
        int16_t  _drift;                         // adaptive drift offset                          (2 bytes)

};