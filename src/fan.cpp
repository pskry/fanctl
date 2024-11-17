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
    this->tachometerCount = 0;
    this->targetSpeed = 0;

    analogWriteFreq(PWM_FREQ);
    analogWriteRange(PWM_DUTY_MAX);
    pinMode(pwmPin, OUTPUT);
    analogWrite(pwmPin, PWM_DUTY_MIN);
    pinMode(tachometerPin, INPUT_PULLUP);
    attachInterruptArg(tachometerPin, &Fan::interruptHandler, this, FALLING);
}
void Fan::setSpeed(const int speed) {
    const uint16 targetSpeed = constrain(speed, PWM_DUTY_MIN, PWM_DUTY_MAX);
    logf("setSpeed: speed=%d, targetSpeed=%d\n", speed, targetSpeed);

    this->targetSpeed = targetSpeed;
}

void Fan::setSpeedPct(const int speedPct) {
    const uint8 clampedPct = constrain(speedPct, FAN_SPEED_PCT_MIN, FAN_SPEED_PCT_MAX);
    const uint16 targetSpeed = PWM_DUTY_MAX * clampedPct / 100;
    logf("setSpeedPct: speedPct=%d, clampedPct=%d, targetSpeed=%d\n", speedPct, clampedPct, targetSpeed);

    this->targetSpeed = targetSpeed;
}

void Fan::update() { analogWrite(pwmPin, targetSpeed); }

uint32 Fan::getTachometerCount() const { return tachometerCount; }
uint16 Fan::getTargetSpeed() const { return targetSpeed; }
void Fan::resetTachometerCount() { tachometerCount = 0; }

void Fan::interruptHandler(void *arg) {
    auto *fan = static_cast<Fan *>(arg);
    fan->tachometerCount++;
}
