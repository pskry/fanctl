//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#ifndef CONFIG_H_
#define CONFIG_H_

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASS"
#define MQTT_SERV "MQTT_SRV_DNS_OR_IP"
#define MQTT_PORT 1883

#define MQTT_FAN_CTL_TOPIC "fanctl"
#define MQTT_RPM_TOPIC "fanrpm"
#define MQTT_PUBLISH_INTERVAL_MS 1000

#define MQTT_BUF_SIZE 256
#define JSON_BUF_SIZE 256

#define SERIAL_BAUD 115200

#define PWM_FREQ 25000
#define PWM_DUTY_MIN 0
#define PWM_DUTY_MAX 16
#define FAN_SPEED_PCT_MIN 0
#define FAN_SPEED_PCT_MAX 100

#endif  // CONFIG_H_
