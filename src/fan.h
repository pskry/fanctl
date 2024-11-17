#ifndef FANCTL_FAN_H_
#define FANCTL_FAN_H_

#include <c_types.h>

class Fan {
private:
    uint8 pwmPin;
    uint8 tachometerPin;
    uint16 targetSpeed;
    uint32 tachometerCount;

public:
    Fan(uint8 pwmPin, uint8 tachometerPin);

    void setSpeed(int speed);
    void setSpeedPct(int speedPct);
    void update();
    [[nodiscard]] uint32 getTachometerCount() const;
    [[nodiscard]] uint16 getTargetSpeed() const;
    void resetTachometerCount();

private:
    static void IRAM_ATTR interruptHandler(void *arg);
};

#define SETUP_FAN(id, pwmPin, tachPin) Fan fan##id(pwmPin, tachPin);
#define FAN(id) fan##id

#endif
