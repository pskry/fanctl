#ifndef FANCTL_FAN_H_
#define FANCTL_FAN_H_

#include <c_types.h>

class Fan {
private:
    uint8 pwmPin;
    uint8 tachometerPin;
    uint16 targetSpeed;
    volatile uint32 *tachometerCounter;

public:
    Fan(uint8 pwmPin, uint8 tachometerPin, volatile uint32 *tachometerCounter, void (*interruptHandler)());

    void setSpeed(uint8 speedPercent);
    void update();
    [[nodiscard]] uint32 getTachometerCount() const;
    void resetTachometerCounter();
};

#define SETUP_FAN(id, pwmPin, tachPin)                       \
    volatile uint32 tachCnt##id = 0;                         \
    void IRAM_ATTR intHandler##id() { tachCnt##id++; } \
    Fan fan##id(pwmPin, tachPin, &tachCnt##id, intHandler##id);

#define FAN(id) fan##id

#endif
