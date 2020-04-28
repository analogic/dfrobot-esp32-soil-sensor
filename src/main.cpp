#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoLog.h>

#define DISABLE_LOGGING

const char* ssid = "****";
const char* password = "****";
const char* server = "****";
const char* topicOut = "****";

#define MSG_BUFFER_SIZE	(5)
#define DEEP_SLEEP_US 60000000
char msg[MSG_BUFFER_SIZE];
int analogPin = A0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Log.notice("Connecting to %s\n", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Log.notice(".\n");
  }

  randomSeed(micros());

  Log.notice("WiFi connected\n");
  Log.notice("IP address: %s\n", WiFi.localIP().toString().c_str());
}

void readAndPublish()
{
  
  const int readingsCount = 20;
  int total = 0; 

  for (int i = 0; i < readingsCount; i++) {
    total += analogRead(analogPin);
  }
  int val = total / readingsCount;
  
  snprintf(msg, MSG_BUFFER_SIZE, "%d", val);

  Log.notice("Publish message: %s\n", msg);
  client.publish(topicOut, msg);
}

void reconnect() {
  while (!client.connected()) {
    Log.notice("Attempting MQTT connection...\n");

    String clientId = "moisture-sensor-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Log.notice("connected\n");
    } else {
      Log.notice("failed, rc=%d try again in 5 seconds\n", client.state());
      delay(5000);
    }
  }
}


void printTimestamp(Print* _logOutput) {
  char c[12];
  sprintf(c, "%10lu ", millis());
  _logOutput->print(c);
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, LOW);

  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  Log.setPrefix(printTimestamp);

  //Start logging

  setup_wifi();

  client.setServer(server, 1883);
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_US);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  readAndPublish();
  digitalWrite(BUILTIN_LED, HIGH);
  esp_deep_sleep_start();
}

void loop() {
  // after deep sleep start it goes to setup again
}