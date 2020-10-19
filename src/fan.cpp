//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#include "fan.hpp"

#include <Arduino.h>

#include "./config.h"
#include "./log.h"

#define CLAMP(val, min, max) (val < min ? min : (val > max ? max : val))
template <typename T>
static inline T clamp(T val, T min, T max) {
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

Fan::Fan(uint8 pwmPin, uint8 tachPin, volatile uint32 *tachCnt,
         void (*intHandler)(void)) {
    if (digitalPinToInterrupt(tachPin) < 0) {
        logf("ERROR: PIN <%d> is NOT an interrupt pin\n", tachPin);
    }

    this->pwmPin = pwmPin;
    this->tachPin = tachPin;
    this->tachCnt = tachCnt;
    this->targetSpeed = 0;

    analogWriteFreq(PWM_FREQ);
    analogWriteRange(PWM_DUTY_MAX);
    pinMode(pwmPin, OUTPUT);
    analogWrite(pwmPin, PWM_DUTY_MIN);
    pinMode(tachPin, INPUT_PULLUP);
    attachInterrupt(tachPin, intHandler, FALLING);
}

void Fan::setSpeed(uint8 pct) {
    uint8 clampedPct = CLAMP(pct, FAN_SPEED_PCT_MIN, FAN_SPEED_PCT_MAX);
    uint16 trgSpd = (PWM_DUTY_MAX * clampedPct) / FAN_SPEED_PCT_MAX;
    logf("pct=%d, clampedPct=%d, trgSpd=%d\n", pct, clampedPct, trgSpd);

    this->targetSpeed = trgSpd;
}

void Fan::update() { analogWrite(pwmPin, targetSpeed); }

int32 Fan::getTachCnt() const { return *tachCnt; }

void Fan::resetTachCnt() { *tachCnt = 0; }
