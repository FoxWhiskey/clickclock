time_t getNtpTime();
void digitalClockDisplay(time_t (*PtimeSrc)());
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
