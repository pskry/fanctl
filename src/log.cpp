//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#ifdef DEBUG
#include <Arduino.h>
#endif

#include "./log.h"

void initLog(void) {
#ifdef DEBUG
    Serial.begin(SERIAL_BAUD);
    while (!Serial) {
        delay(10);
    }
    logf("Serial init done. Baud rate=%d\n", SERIAL_BAUD);
#endif
}
