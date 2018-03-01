/**
 * SlackLight
 * 
 * Author: Casey Fulton <casey@caseyfulton.com>
 * Based on previous work: https://github.com/urish/arduino-slack-bot
 * 
 * 
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// If slack change their cert, look up the new fingerprint here:
// https://www.grc.com/fingerprints.htm (use slack.com as the domain).
#define SLACK_SSL_FINGERPRINT "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3"
#define SLACK_BOT_TOKEN "xoxb-YOUR-TOKEN-HERE"
#define WIFI_SSID "Wifi Network Name"
#define WIFI_PASSWORD "supersecretpassword"

// Used to set time for SSL.
#define HOURS_FROM_UTC 10

// Status lights - hook ALARM_PIN up to your flashing light / siren / whatever.
#define WIFI_PIN D7
#define SLACK_PIN D6
#define ALARM_PIN D5

// Text to look for and freak out about.
#define ALARM_TEXT "ALARM"

// How long to display an alarm for in milliseconds.
#define ALARM_DURATION 3000

// The flicker rate of the alarm in hertz.
#define ALARM_FREQ 5

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool connected = false;
unsigned long alarmEpoch = 0;
unsigned int timeSinceEpoch = 0;
unsigned int period = 1000 / ALARM_FREQ / 2;

/**
  Sends a ping message to Slack. Call this function immediately after establishing
  the WebSocket connection, and then every 5 seconds to keep the connection alive.
*/
void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = ++nextCmdId;
  String json;
  root.printTo(json);
  Serial.printf("[WebSocket] Sending ping: %1u\n", nextCmdId);
  webSocket.sendTXT(json);
}

void processSlackMessage(char *payload) {
  if (strstr(payload, ALARM_TEXT) != NULL) {
    Serial.println("[Alarm] Alarm text seen.");
    alarmEpoch = millis();
  }
}

/**
  Called on each web socket event. Handles disconnection, and also
  incoming messages from slack.
*/
void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      sendPing();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      processSlackMessage((char*)payload);
      break;
  }
}

/**
  Establishes a bot connection to Slack:
  1. Performs a REST call to get the WebSocket URL
  2. Conencts the WebSocket
  Returns true if the connection was established successfully.
*/
bool connectToSlack() {
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.connect)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);
  return true;
}

void setup() {
  Serial.begin(57600);
  Serial.setDebugOutput(true);

  pinMode(WIFI_PIN, OUTPUT);
  pinMode(SLACK_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  digitalWrite(WIFI_PIN, LOW);
  digitalWrite(SLACK_PIN, LOW);
  digitalWrite(ALARM_PIN, LOW);

  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  digitalWrite(WIFI_PIN, HIGH);

  configTime(HOURS_FROM_UTC  * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

unsigned long lastPing = 0;

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void loop() {
  webSocket.loop();

  if (connected) {
    digitalWrite(SLACK_PIN, HIGH);
    // Send ping every 5 seconds, to keep the connection alive.
    if (millis() - lastPing > 5000) {
      sendPing();
      lastPing = millis();
    }
  } else {
    digitalWrite(SLACK_PIN, LOW);
    // Try to connect / reconnect to slack.
    connected = connectToSlack();
    if (!connected) {
      delay(500);
    }
  }

  // If an alarm was triggered, flash the alarm pin.
  if (millis() - alarmEpoch < ALARM_DURATION && ((millis() - alarmEpoch) / period) % 2 == 0) {
    digitalWrite(ALARM_PIN, HIGH);
  } else {
    digitalWrite(ALARM_PIN, LOW);
  }
}

