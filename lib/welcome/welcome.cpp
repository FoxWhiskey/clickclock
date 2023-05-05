#include <Arduino.h>
#ifndef AUTO_VERSION
#define AUTO_VERSION
#endif

const char* welcomemsg =
" ██████╗██╗     ██╗ ██████╗██╗  ██╗     ██████╗██╗      ██████╗  ██████╗██╗  ██╗\r\n\
██╔════╝██║     ██║██╔════╝██║ ██╔╝    ██╔════╝██║     ██╔═══██╗██╔════╝██║ ██╔╝\r\n\
██║     ██║     ██║██║     █████╔╝     ██║     ██║     ██║   ██║██║     █████╔╝ \r\n\
██║     ██║     ██║██║     ██╔═██╗     ██║     ██║     ██║   ██║██║     ██╔═██╗ \r\n\
╚██████╗███████╗██║╚██████╗██║  ██╗    ╚██████╗███████╗╚██████╔╝╚██████╗██║  ██╗\r\n\
 ╚═════╝╚══════╝╚═╝ ╚═════╝╚═╝  ╚═╝     ╚═════╝╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═\r\n\
 A slave clock implementation for ESP8266 by Bernhard Damian 2023.\r\n\
    for Johannes and Helene                                        ";

void welcome() {
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.print(welcomemsg);
    Serial.println(AUTO_VERSION);
    Serial.println();
}