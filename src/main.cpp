//    __                  _   _
//  / _| __ _ _ __   ___| |_| |
// | |_ / _` | '_ \ / __| __| |
// |  _| (_| | | | | (__| |_| |
// |_|  \__,_|_| |_|\___|\__|_|
//
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <c_types.h>

#include "./config.h"
#include "./fan.h"
#include "./log.h"

/* ************************************************************************** */
/* globals                                                                    */
/* ************************************************************************** */
WiFiClient wifiClient;
PubSubClient client(wifiClient);

StaticJsonDocument<JSON_BUF_SIZE> doc;

/* ************************************************************************** */
/* fan-setup                                                                  */
/* ************************************************************************** */
std::array<Fan, 3> fans = {
    Fan(D4, D7),
    Fan(D3, D5),
    Fan(D1, D6),
};

/* ************************************************************************** */
/* setup                                                                      */
/* ************************************************************************** */
static void initWiFi();
static void initMqttClient();
static void ensureMqttConnection();
static void onMqttMessage(const char *topic, byte *payload, unsigned int length);

void setup() {
    initLog();
    initWiFi();
    initMqttClient();
}

void initWiFi() {
    logf("Connecting to %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.hostname(HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        log(".");
    }

    randomSeed(micros());

    log("\nWiFi connected - IP address: ");
    logln(WiFi.localIP());
}

void initMqttClient() {
    client.setBufferSize(MQTT_BUF_SIZE);
    client.setServer(MQTT_HOST, MQTT_PORT);
    client.setCallback(onMqttMessage);
}

/* ************************************************************************** */
/* loop                                                                       */
/* ************************************************************************** */
void loop() {
    static uint32 prev = 0;

    ensureMqttConnection();
    client.loop();

    const uint32 now = millis();
    const uint32 diff = now - prev;
    if (diff >= MQTT_PUBLISH_INTERVAL_MS) {
        char buf[MQTT_BUF_SIZE];
        prev = now;

        constexpr size_t numFans = fans.size();
        uint32 tachometerCounts[numFans];
        uint16 targetSpeeds[numFans];

        // copy fan tachometer counts into local array
        // to keep the critical section as short as possible
        noInterrupts();
        for (size_t i = 0; i < numFans; i++) {
            Fan fan = fans[i];
            tachometerCounts[i] = fan.getTachometerCount();
            targetSpeeds[i] = fan.getTargetSpeed();
            fan.resetTachometerCounter();
            fan.update();
        }
        interrupts();

        doc.clear();

        real64 diffRatio = static_cast<real64>(diff) / static_cast<real64>(MQTT_PUBLISH_INTERVAL_MS);

        doc["info"]["delta_ms"] = diff;
        doc["info"]["delta_ratio"] = diffRatio;
        doc["info"]["interval_ms"] = MQTT_PUBLISH_INTERVAL_MS;
        for (size_t i = 0; i < numFans; ++i) {
            constexpr real64 intervalsPerSecond = 1000.0 / static_cast<real64>(MQTT_PUBLISH_INTERVAL_MS);
            constexpr real64 secondsPerMinute = 60.0;

            uint32 tachometerCount = tachometerCounts[i];
            real64 rotationsPerInt = tachometerCount * diffRatio;
            real64 rotationsPerSec = rotationsPerInt * intervalsPerSecond;
            real64 rotationsPerMin = rotationsPerSec * secondsPerMinute;

            doc["fans"][i]["target_speed"] = targetSpeeds[i];
            doc["fans"][i]["target_speed_pct"] = (targetSpeeds[i] * FAN_SPEED_PCT_MAX) / PWM_DUTY_MAX;
            doc["fans"][i]["tachometer_count"] = tachometerCount;
            doc["fans"][i]["rpi"] = rotationsPerInt;
            doc["fans"][i]["rps"] = rotationsPerSec;
            doc["fans"][i]["rpm"] = rotationsPerMin;
        }

#ifdef LOG_SERIAL
        serializeJsonPretty(doc, buf, MQTT_BUF_SIZE);
#else
        serializeJson(doc, buf, MQTT_BUF_SIZE);
#endif
        logf("publishing to topic '%s' - payload:\n%s\n", MQTT_FANRPM_TOPIC, buf);
        client.publish(MQTT_FANRPM_TOPIC, buf);
    }

    yield();
}

void ensureMqttConnection() {
    if (!client.connected()) {
        logf("Connecting to MQTT server '%s:%d' ...\n", MQTT_HOST, MQTT_PORT);
    }

    while (!client.connected()) {
        if (client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
            logln("Connected.");

            logf("Subscribing to topic '%s' ...", MQTT_FANCTL_TOPIC);
            client.subscribe(MQTT_FANCTL_TOPIC);
            logln(" done.");
        } else {
            logf("Failure. state=%d; retrying in %.2fs...\n",
                 client.state(),
                 MQTT_RETRY_MILLIS / 1000.0);
            delay(MQTT_RETRY_MILLIS);
        }
    }
}

void onMqttMessage(const char *topic, byte *payload, const unsigned int length) {
    (void)topic;
    (void)length;

    logf("Message arrived on topic '%s':\n%s\n",
         topic,
         reinterpret_cast<const char *>(payload));


    if (const DeserializationError err = deserializeJson(doc, payload); err) {
        logf("Failed to deserialize json message: %s - message:\n%s\n",
             err.c_str(),
             reinterpret_cast<const char *>(payload));
        return;
    }

    const uint8 fanId = doc["id"];
    const uint8 speedPct = doc["speed_pct"];
    fans[fanId].setSpeed(speedPct);
}
