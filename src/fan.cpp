//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#include "fan.h"

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

Fan::Fan(const uint8 pwmPin,
         const uint8 tachometerPin,
         volatile uint32 *tachometerCounter,
         void (*interruptHandler)()) {
    if (digitalPinToInterrupt(tachometerPin) < 0) {
        logf("ERROR: PIN <%d> is NOT an interrupt pin\n", tachometerPin);
    }

    this->pwmPin = pwmPin;
    this->tachometerPin = tachometerPin;
    this->tachometerCounter = tachometerCounter;
    this->targetSpeed = 0;

    analogWriteFreq(PWM_FREQ);
    analogWriteRange(PWM_DUTY_MAX);
    pinMode(pwmPin, OUTPUT);
    analogWrite(pwmPin, PWM_DUTY_MIN);
    pinMode(tachometerPin, INPUT_PULLUP);
    attachInterrupt(tachometerPin, interruptHandler, FALLING);
}

void Fan::setSpeed(const uint8 speedPercent) {
    const uint8 clampedPct = CLAMP(speedPercent, FAN_SPEED_PCT_MIN, FAN_SPEED_PCT_MAX);
    const uint16 trgSpd = (PWM_DUTY_MAX * clampedPct) / FAN_SPEED_PCT_MAX;
    logf("pct=%d, clampedPct=%d, trgSpd=%d\n", speedPercent, clampedPct, trgSpd);

    this->targetSpeed = trgSpd;
}

void Fan::update() { analogWrite(pwmPin, targetSpeed); }

uint32 Fan::getTachometerCount() const { return *tachometerCounter; }

void Fan::resetTachometerCounter() { *tachometerCounter = 0; }
