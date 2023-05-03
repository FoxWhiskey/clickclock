#include<Arduino.h>
#include<iostream>
#include<cstdarg>
#include<cstdio>
#include<cstring>
#include "log.h"

using namespace std;

/**
 * @brief sets the system wide loglevel for debug-output
 * @param level string representation of DEBUG|INFO|WARN|FATAL
 */
void loglevelSet(const char* level) {
    
    extern loglevel setloglevel;
  
    if (string(level) == "DEBUG") setloglevel = DEBUG;
    if (string(level) == "INFO") setloglevel = INFO;
    if (string(level) == "WARN") setloglevel = WARN;
    if (string(level) == "FATAL") setloglevel = FATAL;
    
}

/**
 * @brief very basic log facility
 * @param loglevel level the log level
 * @param const char* funname name of the calling function
 * @param const char* format a format string, see sscanf()
 * @param ... a variable list
 * @result void
 */
void log(loglevel level,const char* funname,const char* format, ...) {

  extern loglevel setloglevel;
  const char* logtype;
  size_t bs = 320;
  char buffer[bs];
  string color("");
  va_list Arguments;
  va_start (Arguments,format);
  
  if (level >= setloglevel) {
     switch (level) {
       case DEBUG: logtype = "DEBUG";color.assign(RESET);break;
       case INFO : logtype = "INFO ";color.assign(GREEN);break;
       case WARN : logtype = "WARN ";color.assign(YELLOW);break;
       case FATAL: logtype = "FATAL";color.assign(RED);break;
       default   : logtype = "INFO ";color.assign(GREEN);
     };
  
  snprintf(buffer,bs,"(%s%s%s) %s(): ",color.c_str(),logtype,RESET,funname);
  vsprintf(buffer+strlen(buffer),format,Arguments);
  Serial.println(buffer);
  };
  va_end(Arguments);
};

