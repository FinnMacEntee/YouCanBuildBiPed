#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "JDM";
const char* password = "";

WebServer server(80);

// LED config
const int ledPins[]    = {13, 14, 15, 16};
bool      ledStates[]  = {false, false, false, false};   // logical state: true = ON, false = OFF
bool      activeLow[]  = {false, true, true, true};      // main LED is active-high, RGB are active-low
String    ledNames[]   = {"Main", "Red", "Green", "Blue"};
String    ledColors[]  = {"black", "red", "green", "blue"};

const int LED_COUNT = 4;

// Map logical ON/OFF to actual pin level
void applyLedState(int index) {
  bool on = ledStates[index];
  bool isActiveLow = activeLow[index];

  int level;
  if (isActiveLow) {
    level = on ? LOW : HIGH;   // active-low: LOW = ON
  } else {
    level = on ? HIGH : LOW;   // active-high: HIGH = ON
  }

  digitalWrite(ledPins[index], level);
}

// HTML page
String HTMLPage() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><title>ESP32 LED Control</title></head>";
  html += "<body style='text-align: center; font-family: Arial;'>";
  html += "<h1>ESP32 LED Control</h1>";

  // Status line for each LED
  for (int i = 0; i < LED_COUNT; i++) {
    html += "<p>" + ledNames[i] + " LED is <strong>";
    html += (ledStates[i] ? "ON" : "OFF");
    html += "</strong></p>";
  }

  // Buttons
  for (int i = 0; i < LED_COUNT; i++) {
    if (ledStates[i]) {
      html += "<a href='/off" + String(i) + "' style='padding: 10px 20px; background-color: " 
           + ledColors[i] + "; color: white; text-decoration: none;'>Turn OFF " 
           + ledNames[i] + "</a>&nbsp;";
    } else {
      html += "<a href='/on" + String(i) + "' style='padding: 10px 20px; background-color: " 
           + ledColors[i] + "; color: white; text-decoration: none;'>Turn ON " 
           + ledNames[i] + "</a>&nbsp;";
    }
  }

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", HTMLPage());
}

void handleToggle(int index, bool turnOn) {
  if (index < 0 || index >= LED_COUNT) return;
  ledStates[index] = turnOn;
  applyLedState(index);
  server.send(200, "text/html", HTMLPage());
}

void setup() {
  Serial.begin(115200);

  // Init pins
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    ledStates[i] = false;      // start OFF logically
    applyLedState(i);          // apply correct electrical level
  }

  // Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.println(WiFi.localIP());

  // Routes
  server.on("/", handleRoot);

  // One pair of routes per LED, but using the same handler
  for (int i = 0; i < LED_COUNT; i++) {
    String onPath  = "/on"  + String(i);
    String offPath = "/off" + String(i);

    server.on(onPath.c_str(),  [i]() { handleToggle(i, true);  });
    server.on(offPath.c_str(), [i]() { handleToggle(i, false); });
  }

  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}
