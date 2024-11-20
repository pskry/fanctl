fanctl
===

> Firmware for my homebrew rack fan controller.

Up to 3 fans (or fan-groups) can be connected to the controller.

## Data formats

### `MQTT_FANCTL_TOPIC`

Retrieves commands from MQTT topic `MQTT_FANCTL_TOPIC`
and sets the fan speeds accordingly. This can be done in two ways:

1. Control the speed via absolute PWM duty-cycle values
   (must be between `PWM_DUTY_MIN` and `PWM_DUTY_MAX`)
   ```json
   {
     "id": 0,
     "speed": 8
   }
   ```
2. Control the speed via percent from 0 to 100
   ```json
   {
     "id": 1,
     "speed_pct": 60
   }
   ```

> If both `speed` and `speed_pct` are provided, `speed` takes precedent.

### `MQTT_FANINFO_TOPIC`

Periodically sends status updates to MQTT topic (`MQTT_FANINFO_TOPIC`),
providing the following information:

```json
{
   "general": {
      "delta_ms": 4008,
      "interval_ms": 4000,
      "pwm": {
         "freq": 25000,
         "duty_min": 0,
         "duty_max": 16
      }
   },
   "fans": [
      {
         "target_speed": 12,
         "tachometer_count": 144,
         "rps": 36,
         "rpm": 2156
      },
      {
         "target_speed": 6,
         "tachometer_count": 74,
         "rps": 18,
         "rpm": 1108
      },
      {
         "target_speed": 16,
         "tachometer_count": 156,
         "rps": 39,
         "rpm": 1109
      }
   ]
}
```
