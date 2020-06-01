#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoLog.h>
#include "esp_system.h"

#define DISABLE_LOGGING

const char* ssid = "****";
const char* password = "****";
const char* server = "****";
const char* topicOut = "****;
const char* topicIn = "****";

#define PUMP_DIR 26
#define PUMP_PWM 27
#define PWM_SLOW 50  // arbitrary slow speed PWM duty cycle
#define PWM_FAST 200 // arbitrary fast speed PWM duty cycle
#define PWM_MAX 255 // arbitrary fast speed PWM duty cycle

#define DIR_DELAY 500 // brief delay for abrupt motor changes

#define MSG_BUFFER_SIZE	(5)
#define DEEP_SLEEP_US 60000000

char msg[MSG_BUFFER_SIZE];
int analogPin = A0;

long lastMsg = 0;
long lastWd = 0;
long water = 0;
long watering = 0;

const int wdtTimeout = 15000; //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;

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

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

void setup_watchdog() {
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &resetModule, true);
  timerAlarmWrite(timer, wdtTimeout * 1000, false);
  timerAlarmEnable(timer);
}

void setup_pump() {
  pinMode(PUMP_DIR, OUTPUT);
  pinMode(PUMP_PWM, OUTPUT);
  digitalWrite(PUMP_DIR, LOW);
  digitalWrite(PUMP_PWM, LOW);
}

void start_water() {  
  Log.notice("watering\n");
  // set the motor speed and direction
  digitalWrite(PUMP_DIR, HIGH); // direction = forward
  //analogWrite(PUMP_PWM, 255 - PWM_FAST); // PWM speed = fast
  digitalWrite(PUMP_PWM, LOW );
}

void stop_water() {
  Log.notice("watering stop\n");
  digitalWrite(PUMP_DIR, LOW);
  digitalWrite(PUMP_PWM, LOW);
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
  client.publish(topicOut, msg, true);
}

void reconnect() {
  while (!client.connected()) {
    Log.notice("Attempting MQTT connection...\n");

    String clientId = "moisture-sensor-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Log.notice("connected\n");
      
      client.subscribe(topicIn);

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

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  int seconds = String(messageTemp).toInt();
  Log.notice("received message to water %d seconds\n", seconds);

  if (seconds > 0 && seconds < 100) {
    water = seconds;
  }
}

void setup() {
  setup_pump();
  setup_watchdog();

  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, LOW);

  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  Log.setPrefix(printTimestamp);

  //Start logging
  setup_wifi();

  client.setServer(server, 1883);
  client.setCallback(callback);
  
  // esp_sleep_enable_timer_wakeup(DEEP_SLEEP_US);


  
  
  // esp_deep_sleep_start();
  //delay(2000000);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();

  if (now - lastWd > 5000) {
    timerWrite(timer, 0);
  }

  if (water) {
    if (watering > 0) {
      if (now - watering > water * 1000) {
        stop_water();
        watering = 0;
        client.publish(topicIn, "0");
        water = 0;
      }
    } else {
        start_water();
        watering = now;
    }
  }

  if (now - lastMsg > 60000) {
    lastMsg = now;
    readAndPublish();
  }
}
