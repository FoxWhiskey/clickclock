#include <Arduino.h>

const char* welcomemsg =
" ██████╗██╗     ██╗ ██████╗██╗  ██╗     ██████╗██╗      ██████╗  ██████╗██╗  ██╗\r\n\
██╔════╝██║     ██║██╔════╝██║ ██╔╝    ██╔════╝██║     ██╔═══██╗██╔════╝██║ ██╔╝\r\n\
██║     ██║     ██║██║     █████╔╝     ██║     ██║     ██║   ██║██║     █████╔╝ \r\n\
██║     ██║     ██║██║     ██╔═██╗     ██║     ██║     ██║   ██║██║     ██╔═██╗ \r\n\
╚██████╗███████╗██║╚██████╗██║  ██╗    ╚██████╗███████╗╚██████╔╝╚██████╗██║  ██╗\r\n\
 ╚═════╝╚══════╝╚═╝ ╚═════╝╚═╝  ╚═╝     ╚═════╝╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═\r\n\
 A slave clock implementation for ESP8266 by Bernhard Damian 2023.\r\n\
    for Johannes and Helene\r\n";

void welcome() {
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println(welcomemsg);
}