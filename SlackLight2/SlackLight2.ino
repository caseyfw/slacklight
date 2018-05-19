/**
 * SlackLight2
 *
 * Author: Casey Fulton <casey@caseyfulton.com>
 * Based on previous work: https://github.com/urish/arduino-slack-bot
 *
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

// If slack change their cert, look up the new fingerprint here:
// https://www.grc.com/fingerprints.htm (use slack.com as the domain).
#define SLACK_SSL_FINGERPRINT "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3"
#define SLACK_BOT_TOKEN "xoxb-YOUR-TOKEN-HERE"
#define WIFI_SSID "Wifi Network Name"
#define WIFI_PASSWORD "supersecretpassword"

// Used to set time for SSL.
#define HOURS_FROM_UTC 10

// Text to look for and freak out about.
#define ALARM_TEXT "ALARM"

// How long to display an alarm for in milliseconds.
#define ALARM_DURATION 3000

// Alarm cycle time in milliseconds. Smaller numbers yields a faster spin.
#define ALARM_PERIOD 1500

#define PIXEL_COUNT 8

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

unsigned long lastPing = 0;
unsigned long pingId = 1;
bool connected = false;
unsigned long alarmEpoch = 0;
unsigned int timeSinceEpoch = 0;

// How bright the LEDs are. 0.5f is full stick and probably a bad idea.
const float lightness = 0.3f;

// The RGB LED manager.
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PIXEL_COUNT);

// Gamma correction library - makes fades look like you'd expect.
NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelAnimator animations(1);

/**
 * Sends a ping message to Slack. Call this function immediately after establishing
 * the WebSocket connection, and then every 5 seconds to keep the connection alive.
 */
void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = pingId++;
  String json;
  root.printTo(json);
  Serial.printf("[WebSocket] Sending ping: %1u\n", pingId);
  webSocket.sendTXT(json);
}

/**
 * Scans a slack message and kicks off an alarm if necessary.
 */
void processSlackMessage(char *payload) {
  if (strstr(payload, ALARM_TEXT) != NULL) {
    Serial.println("[Alarm] Alarm text seen.");
    alarmEpoch = millis();
    animations.StartAnimation(0, ALARM_PERIOD, rainbowLoop);
  }
}

/**
 * Called on each web socket event. Handles disconnection, and also
 * incoming messages from slack.
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
 * Establishes a bot connection to Slack:
 * 1. Performs a REST call to get the WebSocket URL
 * 2. Conencts the WebSocket
 * Returns true if the connection was established successfully.
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

/**
 * Pretty rainbow animation.
 */
void rainbowLoop(const AnimationParam& param)
{
  for (unsigned int index = 0; index < strip.PixelCount(); index++)
  {
    // Set each pixel to a corresponding position on the colour wheel.
    float hue = index / (float) strip.PixelCount();

    // Shift the hue as the animation progresses.
    hue = hue + param.progress;

    // Ensure hue is always between 0.0 and 1.0.
    if (hue > 1.0f) {
      hue = hue - 1.0f;
    }

    HslColor hsl = HslColor(hue, 1.0f, lightness);

    // Correct colour gamma (need to go via RgbColor due to api limitations).
    RgbColor color = colorGamma.Correct(RgbColor(hsl));

    strip.SetPixelColor(index, color);
  }

  // Loop the animation.
  if (param.state == AnimationState_Completed)
  {
    animations.RestartAnimation(param.index);
  }
}

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void slackLoop() {
  if (connected) {
    // Send ping every 5 seconds, to keep the connection alive.
    if (millis() - lastPing > 5000) {
      sendPing();
      lastPing = millis();
    }
  } else {
    // Try to connect / reconnect to slack.
    connected = connectToSlack();
    if (!connected) {
      // This should be the only delay in the main loop.
      delay(500);
    }
  }
}

void setup() {
  Serial.begin(57600);
  Serial.setDebugOutput(true);

  // Fire up the LEDs and kick off the rainbow animation.
  strip.Begin();
  animations.StartAnimation(0, ALARM_PERIOD, rainbowLoop);

  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (WiFiMulti.run() != WL_CONNECTED) {
    // Call wifi.run every 500ms, but update animations every 10ms.
    for(int i = 0; i < 50; i++) {
      animations.UpdateAnimations();
      strip.Show();
      delay(10);
    }
  }

  // Turn off the lights.
  animations.StopAnimation(0);
  strip.ClearTo(RgbColor(0));
  strip.Show();

  configTime(HOURS_FROM_UTC  * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  webSocket.loop();

  animations.UpdateAnimations();
  strip.Show();

  slackLoop();

  // Check to see if an alarm has expired.
  if (millis() - alarmEpoch > ALARM_DURATION) {
    // Turn off the lights.
    animations.StopAnimation(0);
    strip.ClearTo(RgbColor(0));
    strip.Show();
  }
}
