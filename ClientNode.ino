// ClientNode.ino
// ESP32 client for HC-SR04 -> sends distance to host AP via HTTP GET
// Set NODE_ID (1..3) before uploading to each node

#include <WiFi.h>

///////// CONFIG //////////
const char* SSID = "WASTE_AP";          // Host AP SSID
const char* PASS = "wastepass";         // Host AP password
IPAddress HOST_IP(192,168,4,1);         // default softAP IP of ESP32 host
const uint16_t HOST_PORT = 80;

const int TRIG_PIN = 25;  // output
const int ECHO_PIN = 34;  // input (use input-only pins 34/35/36)
const int NODE_ID = 1;    // CHANGE per node: 1, 2, 3
const float BIN_HEIGHT_CM = 42.0; // set your bin height in cm here

const unsigned long SEND_INTERVAL_MS = 15000UL; // how often to send readings
///////////////////////////

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  delay(50);

  Serial.printf("Node %d starting, bin height %.1f cm\n", NODE_ID, BIN_HEIGHT_CM);

  WiFi.begin(SSID, PASS);
  Serial.print("Connecting to host AP");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    Serial.print('.');
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to AP, IP:");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nCouldn't connect to AP now - will retry in loop.");
  }
  randomSeed(analogRead(0));
  lastSend = millis() + random(0, 2000); // initial jitter
}

float measureDistanceCm() {
  // send 10 us pulse on TRIG
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // timeout 30ms
  if (duration == 0) return -1.0; // no echo
  // speed of sound formula -> cm
  float distance = duration / 58.0; // approximate
  return distance;
}

void sendReading(float distanceCm) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SSID, PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) delay(200);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi reconnect failed; skipping send.");
      return;
    }
  }

  WiFiClient client;
  if (!client.connect(HOST_IP, HOST_PORT)) {
    Serial.println("Connect to host failed");
    return;
  }

  char url[120];
  if (distanceCm < 0) {
    snprintf(url, sizeof(url), "/update?node=%d&dist=-1", NODE_ID);
  } else {
    // include height for host to compute percentage (optional)
    snprintf(url, sizeof(url), "/update?node=%d&dist=%.1f&height=%.1f", NODE_ID, distanceCm, BIN_HEIGHT_CM);
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: 192.168.4.1\r\n" +
               "Connection: close\r\n\r\n");

  // read response headers minimally
  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 2000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      // (optional) print response
      // Serial.println(line);
      timeout = millis();
    }
  }
  client.stop();
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    float distance = measureDistanceCm();
    if (distance < 0) {
      Serial.println("No echo (object out of range)");
    } else {
      Serial.printf("Distance: %.1f cm\n", distance);
    }
    sendReading(distance);
    lastSend = millis() + random(0, 2000); // small jitter to reduce collisions
  }
  delay(20);
}