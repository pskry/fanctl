//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#ifndef LOG_H_
#define LOG_H_

#ifdef DEBUG
#include <Arduino.h>
#endif

#include "./config.h"

void initLog(void);

#ifdef DEBUG
#define log(arg) Serial.print(arg)
#define logln(arg) Serial.println(arg)
#define logf(fmt, args...) Serial.printf(fmt, args)
#else
#define log(arg)                                                               \
    do {                                                                       \
    } while (0)
#define logln(arg)                                                             \
    do {                                                                       \
    } while (0)
#define logf(fmt, args...)                                                     \
    do {                                                                       \
    } while (0)
#endif

#endif  // LOG_H_
