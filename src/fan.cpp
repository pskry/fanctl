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

Fan::Fan(const uint8 pwmPin, const uint8 tachometerPin) {
    if (digitalPinToInterrupt(tachometerPin) < 0) {
        logf("ERROR: PIN <%d> is NOT an interrupt pin\n", tachometerPin);
    }

    this->pwmPin = pwmPin;
    this->tachometerPin = tachometerPin;
    this->tachometerCounter = 0;
    this->targetSpeed = 0;

    analogWriteFreq(PWM_FREQ);
    analogWriteRange(PWM_DUTY_MAX);
    pinMode(pwmPin, OUTPUT);
    analogWrite(pwmPin, PWM_DUTY_MIN);
    pinMode(tachometerPin, INPUT_PULLUP);
    attachInterruptArg(tachometerPin, &Fan::interruptHandler, this, FALLING);
}

void Fan::setSpeed(const uint8 speedPercent) {
    const uint8 clampedPct = constrain(speedPercent, FAN_SPEED_PCT_MIN, FAN_SPEED_PCT_MAX);
    const uint16 targetSpeed = (PWM_DUTY_MAX * clampedPct) / FAN_SPEED_PCT_MAX;
    logf("pct=%d, clampedPct=%d, targetSpeed=%d\n", speedPercent, clampedPct, targetSpeed);

    this->targetSpeed = targetSpeed;
}

void Fan::update() { analogWrite(pwmPin, targetSpeed); }

uint32 Fan::getTachometerCount() const { return tachometerCounter; }
uint16 Fan::getTargetSpeed() const { return targetSpeed; }

void Fan::resetTachometerCounter() { tachometerCounter = 0; }

void Fan::interruptHandler(void *arg) {
    auto *fan = static_cast<Fan *>(arg);
    fan->tachometerCounter++;
}
