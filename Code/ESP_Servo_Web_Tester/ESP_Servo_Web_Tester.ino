#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
// Direct LEDC API — no ESP32Servo library.
// Requires Arduino ESP32 core 3.0+ (Nano ESP32 package).

// ─── WiFi ────────────────────────────────────────────────────────
const char* ssid     = "JDM";
const char* password = "";

WebServer server(80);

// ─── Servo config ────────────────────────────────────────────────
const int SERVO_COUNT = 5;

const int SERVO_PINS[SERVO_COUNT]     = {47, 21, 18, 17, 38};
// Explicit LEDC hardware channels 0-4 (ESP32-S3 has channels 0-7).
// Hard-coding these avoids the auto-allocator that produced channels 100-104.
const int SERVO_CHANNELS[SERVO_COUNT] = { 0,  1,  2,  3,  4};

const char* SERVO_LABELS[SERVO_COUNT] = {
  "S1  GPIO47  ch0  R-Upper",
  "S2  GPIO21  ch1  R-Lower",
  "S3  GPIO18  ch2  L-Upper",
  "S4  GPIO17  ch3  L-Lower",
  "S5  GPIO38  ch4  Hips"
};

const int  SERVO_FREQ    = 50;    // Hz
const int  SERVO_RES     = 16;    // bits  (65536 counts = 20 000 µs period)
const int  SERVO_MIN_US  = 500;
const int  SERVO_MAX_US  = 2400;

bool servoAttached[SERVO_COUNT] = {false, false, false, false, false};
int  servoLastUs[SERVO_COUNT]   = {1500, 1500, 1500, 1500, 1500};

// Convert microseconds → 16-bit LEDC duty value for 50 Hz.
// Period = 20 000 µs = 65536 counts  →  1 count ≈ 0.305 µs
uint32_t usToDuty(int us) {
  return (uint32_t)constrain(us, SERVO_MIN_US, SERVO_MAX_US)
         * (1 << SERVO_RES) / 20000;
}

// ─── Debug log (circular buffer) ─────────────────────────────────
const int LOG_MAX = 80;
String    logBuf[LOG_MAX];
int       logHead  = 0;   // next write slot
int       logCount = 0;   // number of valid entries

void addLog(const String& msg) {
  Serial.println(msg);
  logBuf[logHead] = msg;
  logHead = (logHead + 1) % LOG_MAX;
  if (logCount < LOG_MAX) logCount++;
}

// Returns all log entries oldest-first as one newline-separated string.
String getLogText() {
  String out;
  out.reserve(logCount * 64);
  int start = (logCount < LOG_MAX) ? 0 : logHead;
  for (int i = 0; i < logCount; i++) {
    out += logBuf[(start + i) % LOG_MAX];
    out += '\n';
  }
  return out;
}

// ─── HTML page builder ───────────────────────────────────────────
String buildPage() {
  String html;
  html.reserve(5120);

  // ── Head & styles ──
  html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
           "<title>ESP32 Servo Tester</title><style>"
           "body{font-family:Arial,sans-serif;max-width:900px;margin:20px auto;padding:0 12px}"
           "h1{margin-bottom:4px}h2{margin:18px 0 6px}"
           "table{border-collapse:collapse;width:100%}"
           "th,td{border:1px solid #bbb;padding:7px 10px;text-align:left;vertical-align:middle}"
           "th{background:#eee}"
           ".btn{padding:6px 14px;cursor:pointer;border:none;border-radius:4px;"
                "color:#fff;font-size:13px;white-space:nowrap}"
           ".att{background:#2196F3}.det{background:#f44336}"
           ".wr{background:#4CAF50}.ref{background:#607D8B}"
           ".on{color:green;font-weight:bold}.off{color:#aaa}"
           "#logbox{width:100%;height:200px;font-family:monospace;font-size:12px;"
                  "background:#111;color:#0f0;border:1px solid #444;"
                  "box-sizing:border-box;resize:vertical;padding:4px}"
           "input[type=number]{width:72px;padding:4px;font-size:13px}"
           "form{margin:0}"
           "</style></head><body>"
           "<h1>ESP32 Servo Tester</h1>\n");

  // ── Servo table ──
  html += F("<h2>Servo Control</h2>"
            "<table>"
            "<tr><th>Servo</th><th>Status</th>"
            "<th>Attach / Detach</th>"
            "<th>Microseconds &amp; Write</th></tr>\n");

  for (int i = 0; i < SERVO_COUNT; i++) {
    int n = i + 1;
    html += "<tr>";

    // Name
    html += "<td>" + String(SERVO_LABELS[i]) + "</td>";

    // Status
    if (servoAttached[i]) {
      html += "<td class='on'>ATTACHED&nbsp;(" + String(servoLastUs[i]) + "&nbsp;us)</td>";
      html += "<td><a href='/detach?s=" + String(n) + "'>"
              "<button class='btn det'>Detach S" + String(n) + "</button></a></td>";
    } else {
      html += "<td class='off'>detached</td>";
      html += "<td><a href='/attach?s=" + String(n) + "'>"
              "<button class='btn att'>Attach S" + String(n) + "</button></a></td>";
    }

    // Write form (disabled styling when detached)
    html += "<td>"
            "<form action='/write' method='get'>"
            "<input type='hidden' name='s' value='" + String(n) + "'>"
            "<input type='number' name='us' value='" + String(servoLastUs[i]) + "'"
            " min='" + String(SERVO_MIN_US) + "'"
            " max='" + String(SERVO_MAX_US) + "'>"
            "&nbsp;<button class='btn wr' type='submit'>Write S" + String(n) + "</button>"
            "</form></td>";

    html += "</tr>\n";
  }
  html += "</table>\n";

  // ── Write all attached ──
  html += F("<h2>Write All Attached Servos</h2>"
            "<form action='/writeall' method='get'>"
            "<input type='number' name='us' value='1500'"
            " min='500' max='2400'>"
            "&nbsp;<button class='btn wr' type='submit'>Write All Attached</button>"
            "</form>\n");

  // ── Debug log ──
  html += F("<h2>Debug Log &nbsp;"
            "<a href='/'><button class='btn ref'>Refresh Page</button></a>"
            "</h2>"
            "<textarea id='logbox' readonly>");
  html += getLogText();
  html += F("</textarea>\n"
            // Scroll textarea to bottom on load
            "<script>"
            "var lb=document.getElementById('logbox');"
            "lb.scrollTop=lb.scrollHeight;"
            "</script>\n");

  html += F("</body></html>");
  return html;
}

// ─── Helper: 302 redirect back to / ─────────────────────────────
void redirectHome() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

// ─── Route handlers ──────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleAttach() {
  if (!server.hasArg("s")) { server.send(400, "text/plain", "missing s"); return; }
  int n = server.arg("s").toInt();
  if (n < 1 || n > SERVO_COUNT) { server.send(400, "text/plain", "s out of range"); return; }
  int i = n - 1;

  if (servoAttached[i]) {
    addLog(">> Servo " + String(n) + " already attached - skipped.");
  } else {
    // ledcAttachChannel assigns this pin exclusively to the given
    // hardware LEDC channel. Returns true on success.
    bool ok = ledcAttachChannel(SERVO_PINS[i], SERVO_FREQ, SERVO_RES,
                                SERVO_CHANNELS[i]);
    servoAttached[i] = ok;
    // Write a neutral 1500 µs pulse immediately so the servo holds centre.
    if (ok) ledcWrite(SERVO_PINS[i], usToDuty(servoLastUs[i]));
    addLog(">> Servo " + String(n) +
           " ledcAttachChannel(pin=GPIO" + String(SERVO_PINS[i]) +
           ", ch=" + String(SERVO_CHANNELS[i]) +
           ") -> " + (ok ? "OK  pulse=" + String(servoLastUs[i]) + "us"
                         : "FAILED"));
  }
  redirectHome();
}

void handleDetach() {
  if (!server.hasArg("s")) { server.send(400, "text/plain", "missing s"); return; }
  int n = server.arg("s").toInt();
  if (n < 1 || n > SERVO_COUNT) { server.send(400, "text/plain", "s out of range"); return; }
  int i = n - 1;

  if (!servoAttached[i]) {
    addLog(">> Servo " + String(n) + " already detached - skipped.");
  } else {
    // ledcDetach removes the GPIO matrix connection and stops PWM output.
    ledcDetach(SERVO_PINS[i]);
    servoAttached[i] = false;
    addLog(">> Servo " + String(n) + " detached"
           "  pin=GPIO" + String(SERVO_PINS[i]) +
           "  ch=" + String(SERVO_CHANNELS[i]));
  }
  redirectHome();
}

void handleWrite() {
  if (!server.hasArg("s") || !server.hasArg("us")) {
    server.send(400, "text/plain", "missing args"); return;
  }
  int n  = server.arg("s").toInt();
  int us = server.arg("us").toInt();
  if (n < 1 || n > SERVO_COUNT) { server.send(400, "text/plain", "s out of range"); return; }
  us = constrain(us, SERVO_MIN_US, SERVO_MAX_US);
  int i = n - 1;

  if (!servoAttached[i]) {
    addLog(">> Servo " + String(n) + " NOT attached - write ignored.");
  } else {
    uint32_t duty = usToDuty(us);
    ledcWrite(SERVO_PINS[i], duty);
    servoLastUs[i] = us;
    addLog(">> Servo " + String(n) +
           " ledcWrite(ch=" + String(SERVO_CHANNELS[i]) +
           ", duty=" + String(duty) +
           ")  =" + String(us) + "us");
  }
  redirectHome();
}

void handleWriteAll() {
  if (!server.hasArg("us")) { server.send(400, "text/plain", "missing us"); return; }
  int us = constrain(server.arg("us").toInt(), SERVO_MIN_US, SERVO_MAX_US);
  int wrote = 0;
  for (int i = 0; i < SERVO_COUNT; i++) {
    if (servoAttached[i]) {
      uint32_t duty = usToDuty(us);
      ledcWrite(SERVO_PINS[i], duty);
      servoLastUs[i] = us;
      wrote++;
      addLog(">> Servo " + String(i + 1) +
             " ledcWrite(ch=" + String(SERVO_CHANNELS[i]) +
             ", duty=" + String(duty) +
             ")  =" + String(us) + "us");
    }
  }
  if (wrote == 0) addLog(">> writeAll: no servos attached - nothing written.");
  redirectHome();
}

// ─── Setup ───────────────────────────────────────────────────────
void setup() {
  delay(10);
  Serial.begin(115200);
  if(!Serial) delay(1000);
  // No while(!Serial) — board must run headless without USB.

  addLog("=== ESP32 Servo Tester boot ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  addLog(String("Connecting to: ") + ssid);

  // Wait up to 20 s for WiFi; do not block forever so the board stays usable.
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addLog("WiFi connected.  IP: " + WiFi.localIP().toString());
    if (MDNS.begin("esp32servo")) {
      addLog("mDNS started.  URL: http://esp32servo.local");
    } else {
      addLog("mDNS failed - use IP address above.");
    }
  } else {
    addLog("WiFi FAILED after " + String(attempts) + " attempts. Server unreachable.");
  }

  // Using direct LEDC API - no library timer allocation needed.
  // Channels 0-4 are hard-assigned above in SERVO_CHANNELS[].
  addLog("Direct LEDC mode. Servo channels: 0=S1 1=S2 2=S3 3=S4 4=S5");

  server.on("/",         handleRoot);
  server.on("/attach",   handleAttach);
  server.on("/detach",   handleDetach);
  server.on("/write",    handleWrite);
  server.on("/writeall", handleWriteAll);
  server.begin();
  addLog("Web server started on port 80.");
  addLog("=== Ready ===");
}

// ─── Loop ────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
}
