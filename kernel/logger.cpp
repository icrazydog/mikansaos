/**
 * @file logger.cpp
 * logger
 */

#include "logger.hpp"

//#include <cstddef>
#include <cstdio>

#include "console.hpp"

namespace{
  LogLevel log_level = kWarn;
}

extern Console* console;

void SetLogLevel(LogLevel level){
  log_level = level;
}

static bool logging = false;
int Log(LogLevel level, const char* format, ...){

  if(level>log_level){
    return 0;
  }
  if(logging){
    return 0;
  }
  logging = true;

  va_list ap;
  char s[1024];

  va_start(ap, format);
  vsprintf(s, format, ap);
  va_end(ap);

  int result = printk(s);
  logging =false;
  return result;

}

int Log(LogLevel level,Error err,const char* format, ...){
  va_list ap;
  int result;
  char formatWithLineInfo[1024];

  sprintf(formatWithLineInfo,"%s (at %s:%d)\n" , format, err.File(), err.Line());
  va_start(ap, format);
  result = Log(level,formatWithLineInfo, ap);
  va_end(ap);

  return result;
}
