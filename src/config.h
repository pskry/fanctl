#ifndef FANCTL_CONFIG_H_
#define FANCTL_CONFIG_H_

/* ************************************************************************** */
/* compile-time config                                                        */
/* ************************************************************************** */

#ifndef HOSTNAME
#define HOSTNAME "fanctl"
#endif

#ifndef WIFI_SSID
#error "You must define WIFI_SSID"
#endif
#ifndef WIFI_PASS
#error "You must define WIFI_PASS"
#endif

#ifndef MQTT_HOST
#error "You must define MQTT_HOST"
#endif
#ifndef MQTT_USER
#define MQTT_USER nullptr
#endif
#ifndef MQTT_PASS
#define MQTT_PASS nullptr
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_RETRY_MILLIS
#define MQTT_RETRY_MILLIS 5000
#endif

#ifndef MQTT_FANCTL_TOPIC
#define MQTT_FANCTL_TOPIC "fanctl"
#endif
#ifndef MQTT_FANRPM_TOPIC
#define MQTT_FANRPM_TOPIC "fanrpm"
#endif
#ifndef MQTT_PUBLISH_INTERVAL_MS
#define MQTT_PUBLISH_INTERVAL_MS 1000
#endif

#define BUF_SIZE 1024

#define SERIAL_BAUD 115200

#define FAN_UPDATE_INTERVAL_MS 100
#define PWM_FREQ 25000
#define PWM_DUTY_MIN 0
#define PWM_DUTY_MAX 16
#define FAN_SPEED_PCT_MIN 0
#define FAN_SPEED_PCT_MAX 100

#endif
