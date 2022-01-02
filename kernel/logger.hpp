#pragma once

#ifndef __mikanuser
#include <cstdio>

#include "error.hpp"
#include "console.hpp"
#endif

enum LogLevel{
  kError = 3,
  kWarn = 4,
  kInfo = 6,
  kDebug = 7,
  kDebugMass = 8,
};

void SetLogLevel(enum LogLevel level);

int Log(enum LogLevel level, const char* format, ...);


#ifndef __mikanuser
int printk(const char* format, ...);

int Log(enum LogLevel level,Error err,const char* format, ...);
#endif
