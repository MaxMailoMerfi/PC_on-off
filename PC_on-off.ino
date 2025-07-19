// Arduino Maker Workshop
// Щоб створити файли для праці і обновити [bin], натисніть [Complite] внизу праворуч
// файл bin копіюється натисканням [Ctrl+Shift+B] та вибором [build+copy] і знаходиться поруч з ino

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ---------------- Параметри Wi-Fi ----------------
const char *wifiList[][2] = {
    {"deti_podzemelia", "12345678"},
    {"Xiaomi 14T", ""},
    {"Ingener_Technology", ""}};
int rowsWifiList = sizeof(wifiList) / sizeof(wifiList[0]);

// ---------------- Піни ----------------
#define RELE_PIN 4   // D2
#define POWER_PIN A0 // Analog

// ---------------- Стани ----------------
unsigned long workTime = 0, onPcTime = 0, lastMillis = 0;
unsigned long OffPC = 0, OnPC = 0, lastSave = 0, lastIpPrint = 0;
int POWER = 0, OnOffPower = 0;
const float POWER_THRESHOLD = 3.0;
const unsigned long SAVE_INTERVAL = 10000, IP_PRINT_INTERVAL = 1000;

ESP8266WebServer server(80);
String deviceName = "Головний комп'ютер";

// ---------------- Прототипи ----------------
void connectWiFi(), saveData(), loadData(), deleteJSON(const char *);
void handleRoot(), handleAPIState(), handleRele(), handleReset(), handleClear();
String formatUptime(unsigned long);

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(RELE_PIN, OUTPUT); digitalWrite(RELE_PIN, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);

  if (!LittleFS.begin()) Serial.println("[FS] ❌ Помилка ініціалізації");

  loadData();
  
  connectWiFi();

  int raw = analogRead(POWER_PIN);
  float voltage = raw / 1024.0f * 5.0f;
  POWER = (voltage > POWER_THRESHOLD) ? 1 : 0;
  OnOffPower = POWER;
  OnPC = POWER ? onPcTime : 0;
  OffPC = !POWER ? workTime - OnPC : 0;

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleAPIState);
  server.on("/api/rele", HTTP_POST, handleRele);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/clear", HTTP_POST, handleClear);
  server.onNotFound([]() {
    server.send(404, "text/plain; charset=utf-8", "Not found");
  });

  server.begin();
  Serial.println("[HTTP] ✅ Сервер запущено");
  lastMillis = millis();
}

// ---------------- LOOP ----------------
void loop() {
  unsigned long now = millis();
  workTime += now - lastMillis;
  lastMillis = now;

  // --- Автоматична перевірка Wi-Fi ---
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] ❌ З'єднання втрачено, повторне підключення...");
    connectWiFi();
  }

  int raw = analogRead(POWER_PIN);
  float voltage = raw / 1024.0f * 5.0f;
  POWER = (voltage > POWER_THRESHOLD) ? 1 : 0;

  if (POWER) OnPC = workTime - OffPC;
  else OffPC = workTime - OnPC;
  onPcTime = OnPC;

  if (POWER != OnOffPower) {
    OnOffPower = POWER;
    Serial.println(POWER ? "[STATE] ✅ Увімкнувся" : "[STATE] ❌ Вимкнувся");
  }

  if (now - lastSave >= SAVE_INTERVAL) {
    saveData(); lastSave = now;
  }

  if (now - lastIpPrint >= IP_PRINT_INTERVAL) {
    lastIpPrint = now;
    IPAddress ip = WiFi.localIP();
    if (ip[0] == 0) Serial.println("[WiFi] Поточний IP: (IP unset)");
    else {
      Serial.print("[WiFi] Поточний IP: ");
      Serial.println(ip);
    }
  }

  server.handleClient();
}

// ---------------- Wi-Fi ----------------
void connectWiFi() {
  Serial.println("[WiFi] Пошук мереж...");
  for (int i = 0; i < rowsWifiList; i++) {
    Serial.print("[WiFi] Спроба: ");
    Serial.println(wifiList[i][0]);
    WiFi.begin(wifiList[i][0], wifiList[i][1]);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 7000) {
      delay(500);
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] ✅ Підключено до ");
      Serial.println(WiFi.SSID());
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
      digitalWrite(LED_BUILTIN, LOW);
      return;
    }
  }
  Serial.println("[WiFi] ❌ Не вдалося підключитись. Рестарт...");
  delay(2000); ESP.restart();
}

// ---------------- FS ----------------
void saveData() {
  StaticJsonDocument<256> doc;
  doc["workTime"] = workTime;
  doc["onPcTime"] = onPcTime;
  File f = LittleFS.open("/data.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
    Serial.println("[FS] ✅ Дані збережено");
  } else {
    Serial.println("[FS] ❌ Помилка запису");
  }
}

void loadData() {
  if (!LittleFS.exists("/data.json")) {
    Serial.println("[FS] data.json не знайдено – створюю");
    saveData(); return;
  }
  File f = LittleFS.open("/data.json", "r");
  if (!f) { Serial.println("[FS] ❌ Помилка читання"); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, f)) {
    Serial.println("[FS] ❌ JSON помилка");
  } else {
    workTime = doc["workTime"] | 0;
    onPcTime = doc["onPcTime"] | 0;
    Serial.println("[FS] ✅ Дані завантажено");
  }
  f.close();
}

void deleteJSON(const char *path) {
  if (LittleFS.exists(path) && LittleFS.remove(path))
    Serial.println("[FS] JSON видалено");
  else
    Serial.println("[FS] ❌ Не вдалося видалити");
}

// ---------------- API ----------------
void handleRoot() {
  String html = F(R"rawliteral(
<!DOCTYPE html><html lang='uk'><head><meta charset='utf-8'/>
<meta name='viewport' content='width=device-width,initial-scale=1'/>
<title>)rawliteral");
  html += deviceName;
  html += F(R"rawliteral(</title><style>body{font-family:sans-serif;background:#111;color:#eee;margin:14px;}h1{font-size:20px;}button{padding:10px;margin:4px;background:#3a6aff;color:#fff;border:none;border-radius:6px;}.stat{margin:8px 0;padding:8px;background:#222;border-radius:6px;}#status.on{color:#6fd96f;}#status.off{color:#ff6b6b;}</style></head><body><h1>)rawliteral");
  html += deviceName;
  html += F(R"rawliteral( – Панель</h1><div class='stat'>Стан: <span id='status'>...</span><br>Напруга: <span id='voltage'>...</span> V<br>Аптайм: <span id='uptime'>...</span><br>Час 'ON': <span id='onTime'>...</span><br>Час 'OFF': <span id='offTime'>...</span></div><div><button onclick="sendAct('on')">Увімкнути</button><button onclick="sendAct('off')">Вимкнути</button><button onclick="resetDev()">Рестарт</button><button onclick="clearData()">Очистити</button></div><script>async function load(){const r=await fetch('/api/state');if(!r.ok)return;const j=await r.json();document.getElementById('status').textContent=j.power?'ПРАЦЮЄ ✅':'ВИМКНЕНO ❌';document.getElementById('status').className=j.power?'on':'off';document.getElementById('voltage').textContent=j.voltage.toFixed(2);document.getElementById('uptime').textContent=j.uptime;document.getElementById('onTime').textContent=j.onTime;document.getElementById('offTime').textContent=j.offTime;}function sendAct(a){fetch('/api/rele?action='+a,{method:'POST'}).then(()=>setTimeout(load,400));}function resetDev(){if(confirm('Рестарт пристрою?'))fetch('/api/reset',{method:'POST'});}function clearData(){if(confirm('Очистити дані та рестарт?'))fetch('/api/clear',{method:'POST'});}setInterval(load,2000);load();</script></body></html>)rawliteral");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleAPIState() {
  int raw = analogRead(POWER_PIN);
  float voltage = raw / 1024.0f * 5.0f;
  StaticJsonDocument<256> doc;
  doc["power"] = POWER;
  doc["voltage"] = voltage;
  doc["uptime"] = formatUptime(workTime);
  doc["onTime"] = formatUptime(OnPC);
  doc["offTime"] = formatUptime(OffPC);
  doc["ssid"] = WiFi.SSID();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleRele() {
  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "Missing action"); return;
  }
  String a = server.arg("action");
  if (a == "on" || a == "off") {
    digitalWrite(RELE_PIN, LOW); delay(500); digitalWrite(RELE_PIN, HIGH);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad action");
  }
}

void handleReset() {
  server.send(200, "text/plain", "Rebooting"); delay(200); ESP.restart();
}

void handleClear() {
  deleteJSON("/data.json");
  server.send(200, "text/plain", "Cleared"); delay(300); ESP.restart();
}

// ---------------- Формат часу ----------------
String formatUptime(unsigned long ms) {
  unsigned long s = ms / 1000;
  unsigned int sec = s % 60, min = (s / 60) % 60, hour = (s / 3600) % 24;
  unsigned long day = s / 86400;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lu діб %02u:%02u:%02u", day, hour, min, sec);
  return String(buf);
}
