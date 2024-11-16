#ifdef LOG_SERIAL
#include <Arduino.h>
#endif

#include "./log.h"

void initLog() {
#ifdef LOG_SERIAL
    Serial.begin(SERIAL_BAUD);
    Serial.setDebugOutput(true);
    while (!Serial) {
        delay(10);
    }
    logf("Serial init done. Baud rate=%d\n", SERIAL_BAUD);
#endif
}
