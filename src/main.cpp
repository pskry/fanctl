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

/* ************************************************************************** */
/* fan-setup                                                                  */
/* ************************************************************************** */
std::array<Fan, 3> fans = {
    Fan(D1, D6),
    Fan(D3, D5),
    Fan(D4, D7),
};

/* ************************************************************************** */
/* setup                                                                      */
/* ************************************************************************** */
static void initWiFi();
static void initMqttClient();
static void updateFans();
static void publishMqttMessage(uint32 deltaMillis);
static void ensureMqttConnection();
static void onMqttMessage(const char *topic, byte *payload, uint length);

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
    client.setBufferSize(BUF_SIZE);
    client.setServer(MQTT_HOST, MQTT_PORT);
    client.setCallback(onMqttMessage);
}

/* ************************************************************************** */
/* loop                                                                       */
/* ************************************************************************** */
void loop() {
    static uint32 prevUpdate = millis();
    static uint32 prevPublish = millis();

    ensureMqttConnection();
    client.loop();

    const uint32 now = millis();
    if (const uint32 dUpdate = now - prevUpdate;
        dUpdate >= FAN_UPDATE_INTERVAL_MS) {
        updateFans();
        prevUpdate = now;
    }

    if (const uint32 dPublish = now - prevPublish;
        dPublish >= MQTT_PUBLISH_INTERVAL_MS) {
        publishMqttMessage(dPublish);
        prevPublish = now;
    }

    yield();
}

void updateFans() {
    constexpr size_t numFans = fans.size();

    noInterrupts();
    for (size_t i = 0; i < numFans; i++) {
        Fan &fan = fans[i];
        fan.update();
    }
    interrupts();
}

void publishMqttMessage(const uint32 deltaMillis) {
    constexpr size_t numFans = fans.size();

    StaticJsonDocument<BUF_SIZE> doc;
    char buf[BUF_SIZE];

    uint32 tachometerCounts[numFans];
    uint16 targetSpeeds[numFans];

    // copy fan tachometer counts into local array
    // to keep the critical section as short as possible
    noInterrupts();
    for (size_t i = 0; i < numFans; i++) {
        Fan &fan = fans[i];
        tachometerCounts[i] = fan.getTachometerCount();
        targetSpeeds[i] = fan.getTargetSpeed();
        fan.resetTachometerCount();
    }
    interrupts();

    doc.clear();

    real64 deltaRatio = static_cast<real64>(deltaMillis) /
        static_cast<real64>(MQTT_PUBLISH_INTERVAL_MS);

    doc["info"]["delta_ms"] = deltaMillis;
    doc["info"]["interval_ms"] = MQTT_PUBLISH_INTERVAL_MS;
    doc["info"]["delta_ratio"] = deltaRatio;
    for (size_t i = 0; i < numFans; ++i) {
        constexpr real64 intervalsPerSecond = 1000.0 /
            static_cast<real64>(MQTT_PUBLISH_INTERVAL_MS);
        constexpr real64 secondsPerMinute = 60.0;

        uint32 tachometerCount = tachometerCounts[i];
        real64 rotationsPerInt = tachometerCount * deltaRatio;
        real64 rotationsPerSec = rotationsPerInt * intervalsPerSecond;
        real64 rotationsPerMin = rotationsPerSec * secondsPerMinute;

        doc["fans"][i]["target_speed"] = targetSpeeds[i];
        doc["fans"][i]["target_speed_pct"] = targetSpeeds[i] * 100 / PWM_DUTY_MAX;
        doc["fans"][i]["tachometer_count"] = tachometerCount;
        doc["fans"][i]["rpi"] = rotationsPerInt;
        doc["fans"][i]["rps"] = rotationsPerSec;
        doc["fans"][i]["rpm"] = rotationsPerMin;
    }

#ifdef LOG_SERIAL
    serializeJsonPretty(doc, buf, BUF_SIZE);
#else
    serializeJson(doc, buf, BUF_SIZE);
#endif
    logf("publishing to topic '%s' - payload:\n%s\n", MQTT_FANRPM_TOPIC, buf);
    client.publish(MQTT_FANRPM_TOPIC, buf);
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

void onMqttMessage(const char *topic, byte *payload, const uint length) {
    (void)topic;

    constexpr size_t numFans = fans.size();

    StaticJsonDocument<BUF_SIZE> doc;

    logf("Message arrived on topic '%s':\n", topic);
    for (uint i = 0; i < length; i++) {
        log(static_cast<char>(payload[i]));
    }
    logln();

    if (const DeserializationError err = deserializeJson(doc, payload); err) {
        logf("Failed to deserialize json message: %s - message:\n%s\n",
             err.c_str(),
             reinterpret_cast<const char *>(payload));
        return;
    }

    if (!doc.containsKey("id")) {
        logln("Invalid FANCTL command: 'id' field is required");
        return;
    }

    const int fanId = doc["id"] | -1;
    if (fanId < 0 || static_cast<uint>(fanId) >= numFans) {
        logf("Invalid fan id '%s' - fan id must be between 0 and %d\n", doc["id"].as<const char *>(), numFans);
        return;
    }

    // we set the speed to 0 if we read anything out
    // of the ordinary from either 'speed' or 'speed_pct'.
    //
    // 'speed' takes precedence.
    if (doc.containsKey("speed")) {
        const int speed = doc["speed"] | 0;
        logf("Setting fan '%d' to speed '%d'\n", fanId, speed);
        fans[fanId].setSpeed(speed);
    } else if (doc.containsKey("speed_pct")) {
        const int speedPct = doc["speed_pct"] | 0;
        logf("Setting fan '%d' to speed_pct '%d'\n", fanId, speedPct);
        fans[fanId].setSpeedPct(speedPct);
    } else {
        logln("Invalid FANCTL command: either 'speed' or 'speed_pct' is required");
    }
}
