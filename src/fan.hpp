//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#ifndef FAN_HPP_
#define FAN_HPP_

#include <c_types.h>

class Fan {
 private:
    uint8 pwmPin;
    uint8 tachPin;
    uint16 targetSpeed;
    volatile uint32 *tachCnt;

 public:
    Fan(uint8 pwmPin, uint8 tachPin, volatile uint32 *tachCnt,
        void (*intHandler)(void));
    void setSpeed(uint8 pct);
    void update(void);
    int32 getTachCnt() const;
    void resetTachCnt();
};

#define SETUP_FAN(id, pwmPin, tachPin)                                         \
    volatile uint32 tachCnt##id = 0;                                           \
    void ICACHE_RAM_ATTR intHandler##id(void) { tachCnt##id++; }               \
    Fan fan##id(pwmPin, tachPin, &tachCnt##id, intHandler##id);

#define FAN(id) fan##id

#endif  // FAN_HPP_
