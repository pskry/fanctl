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
#include "./fan.hpp"
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
static void initWiFi(void);
static void initMqttClient(void);
static void ensureMqttConnection(void);
static void onMqttMessage(char *topic, byte *payload, unsigned int length);

void setup(void) {
    initLog();
    initWiFi();
    initMqttClient();
}

void initWiFi() {
    logf("Connecting to %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
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
    client.setServer(MQTT_SERV, MQTT_PORT);
    client.setCallback(onMqttMessage);
}

/* ************************************************************************** */
/* loop                                                                       */
/* ************************************************************************** */
uint64 prev = 0;
void loop(void) {
    ensureMqttConnection();
    client.loop();

    uint64 now = millis();
    int32 diff = now - prev;
    if (diff >= MQTT_PUBLISH_INTERVAL_MS) {
        char buf[MQTT_BUF_SIZE];
        prev = now;

        size_t numFans = fans.size();
        uint32 tachCnts[numFans];

        noInterrupts();
        for (size_t i = 0; i < numFans; i++) {
            Fan fan = fans[i];
            tachCnts[i] = fan.getTachCnt();
            fan.resetTachCnt();
            fan.update();
        }
        interrupts();

        doc.clear();

        for (size_t i = 0; i < numFans; ++i) {
            int32 tachCnt = tachCnts[i];
            uint32 rps = tachCnt * (diff / MQTT_PUBLISH_INTERVAL_MS);
            uint32 rpm = rps * 60;

            doc["fan"][i]["id"] = i;
            doc["fan"][i]["rpm"] = rpm;
        }

        serializeJsonPretty(doc, buf, MQTT_BUF_SIZE);
        logf("publishing to topic <%s> - payload:\n%s\n", MQTT_RPM_TOPIC, buf);
        client.publish(MQTT_RPM_TOPIC, buf);
    }

    yield();
}

void ensureMqttConnection() {
    if (!client.connected()) {
        logf("Connecting to MQTT server <%s:%d> ...\n", MQTT_SERV, MQTT_PORT);
    }

    while (!client.connected()) {
        String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            logln("Connected.");

            logf("Subscribing to topic <%s> ...", MQTT_FAN_CTL_TOPIC);
            client.subscribe(MQTT_FAN_CTL_TOPIC);
            logln(" done.");
        } else {
            logf("Failure. state=%d; retrying in 5s...\n", client.state());
            delay(5000);
        }
    }
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
    logf("Message arrived on topic <%s> ...\n", topic);
    for (unsigned int i = 0; i < length; i++) {
        log(static_cast<char>(payload[i]));
    }
    logln();

    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        logf("Failed to deserialize json message: %s - message:\n%s\n",
             err.c_str(), payload);
        return;
    }

    uint8 fanId = doc["id"];
    uint8 speedPct = doc["speed_pct"];
    fans[fanId].setSpeed(speedPct);
}
