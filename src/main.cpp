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
SETUP_FAN(1, D4, D7)
SETUP_FAN(2, D3, D5)
SETUP_FAN(3, D1, D6)

std::array<Fan, 3> fans = {FAN(1), FAN(2), FAN(3)};

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
    static uint64 prev = 0;

    ensureMqttConnection();
    client.loop();

    const uint64 now = millis();
    const uint64 diff = now - prev;
    if (diff >= MQTT_PUBLISH_INTERVAL_MS) {
        char buf[MQTT_BUF_SIZE];
        prev = now;

        size_t numFans = fans.size();
        uint32 tachCnts[numFans];

        noInterrupts();
        for (size_t i = 0; i < numFans; i++) {
            Fan fan = fans[i];
            tachCnts[i] = fan.getTachometerCount();
            fan.resetTachometerCounter();
            fan.update();
        }
        interrupts();

        doc.clear();

        for (size_t i = 0; i < numFans; ++i) {
            uint32 tachCnt = tachCnts[i];
            uint32 rps = tachCnt * (diff / MQTT_PUBLISH_INTERVAL_MS);
            uint32 rpm = rps * 60;

            doc["fan"][i]["id"] = i;
            doc["fan"][i]["rpm"] = rpm;
        }

        serializeJsonPretty(doc, buf, MQTT_BUF_SIZE);
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
        String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            logln("Connected.");

            logf("Subscribing to topic '%s' ...", MQTT_FANCTL_TOPIC);
            client.subscribe(MQTT_FANCTL_TOPIC);
            logln(" done.");
        } else {
            logf("Failure. state=%d; retrying in 5s...\n", client.state());
            delay(5000);
        }
    }
}

void onMqttMessage(const char *topic, byte *payload, const unsigned int length) {
    (void)topic;

    logf("Message arrived on topic '%s' ...\n", topic);
    for (unsigned int i = 0; i < length; i++) {
        log(static_cast<char>(payload[i]));
    }
    logln();

    const DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        logf("Failed to deserialize json message: %s - message:\n%s\n", err.c_str(), reinterpret_cast<const char *>(payload));
        return;
    }

    const uint8 fanId = doc["id"];
    const uint8 speedPct = doc["speed_pct"];
    fans[fanId].setSpeed(speedPct);
}
