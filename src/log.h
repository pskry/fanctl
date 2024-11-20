#ifndef FANCTL_LOG_H_
#define FANCTL_LOG_H_

#ifdef LOG_SERIAL
#include <Arduino.h>
#endif

#include "./config.h"

void initLog();

// clang-format off
#ifdef LOG_SERIAL
#define log(arg)           Serial.print(arg)
#define logln(arg)         Serial.println(arg)
#define logf(fmt, ...)     Serial.printf(fmt, __VA_ARGS__)
#else
#define log(arg)           do { } while (0)
#define logln(arg)         do { } while (0)
#define logf(fmt, args...) do { } while (0)
#endif
// clang-format on

#endif
