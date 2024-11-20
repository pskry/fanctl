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

    char buf[BUF_SIZE];

    uint32 tachometerCounts[numFans];
    uint16 targetSpeeds[numFans];

    // we copy fan info into local array in order to
    // keep the critical section as short as possible
    noInterrupts();
    for (size_t i = 0; i < numFans; i++) {
        Fan &fan = fans[i];
        tachometerCounts[i] = fan.getTachometerCount();
        targetSpeeds[i] = fan.getTargetSpeed();
        fan.resetTachometerCount();
    }
    interrupts();

    StaticJsonDocument<BUF_SIZE> doc;
    doc["general"]["delta_ms"] = deltaMillis;
    doc["general"]["interval_ms"] = MQTT_PUBLISH_INTERVAL_MS;
    doc["general"]["pwm"]["freq"] = PWM_FREQ;
    doc["general"]["pwm"]["duty_min"] = PWM_DUTY_MIN;
    doc["general"]["pwm"]["duty_max"] = PWM_DUTY_MAX;
    for (size_t i = 0; i < numFans; ++i) {
        // example:
        // MQTT_PUBLISH_INTERVAL_MS = 1000    // count and report every second
        // deltaMillis              = 998     // loop ran 2ms too quickly
        // tachometerCount          = 20      // the fan completed 20 rotations
        //                                       since the last report
        // ---------------------------------------------------------------------
        // rotPerMs                 = 20 / 998             =~ 0,02004008
        // rotPerSec                = 20 * 1000 / 998      =~ 20,04008016
        // rotPerMin                = 20 * 1000 * 60 / 998 =~ 1202,40480962
        uint32 tachometerCount = tachometerCounts[i];
        real64 rotPerMs = static_cast<real64>(tachometerCount) /
            static_cast<real64>(deltaMillis);
        real64 rotPerSec = rotPerMs * 1000.0;
        real64 rotPerMin = rotPerSec * 60.0;

        doc["fans"][i]["target_speed"] = targetSpeeds[i];
        doc["fans"][i]["tachometer_count"] = tachometerCount;
        doc["fans"][i]["rps"] = rotPerSec;
        doc["fans"][i]["rpm"] = rotPerMin;
    }

#ifdef LOG_SERIAL
    serializeJsonPretty(doc, buf, BUF_SIZE);
#else
    serializeJson(doc, buf, BUF_SIZE);
#endif
    logf("publishing to topic '%s' - payload:\n%s\n", MQTT_FANINFO_TOPIC, buf);
    client.publish(MQTT_FANINFO_TOPIC, buf);
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
