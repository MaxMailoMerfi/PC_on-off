/*
  Переробка Telegram‑бота на веб-інтерфейс для LOLIN (WEMOS) D1 R2 (ESP8266)
  Функції:
    - Підключення до першої доступної Wi-Fi мережі зі списку
    - Веб сторінка з індикацією живлення (POWER), аптаймом, часом "увімкнено" та "очікування"
    - Кнопки Увімкнути / Вимкнути (керування реле імпульсом 500 мс)
    - AJAX оновлення стану кожні 2 сек без перезавантаження сторінки
    - REST API:
        GET  /api/state      -> JSON стан
        POST /api/rele?action=on|off
        POST /api/reset      -> перезапуск
        POST /api/clear      -> очистити data.json + перезапуск
    - Збереження workTime / onPcTime у LittleFS (data.json)

  Примітки безпеки:
    * НЕ зберігайте реальні паролі в прошивці (винесіть у secrets.h або використайте OTA конфігурацію).
    * Видаліть BOT_TOKEN з колишнього коду – він більше не потрібен. (Ваш токен зараз компрометований, змініть його у BotFather.)
*/

// Arduino Maker Workshop
// Щоб створити файли для праці і обновити [bin], натисніть [Complite] внизу праворуч
// файл bin копіюється натисканням [Ctrl+Shift+B] та вибором [build+copy] і знаходиться поруч з ino

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ---------------- Wi-Fi списки ----------------
const char *wifiList[][2] = {
    {"deti_podzemelia", "12345678"},
    {"Xiaomi 14T", ""},
    {"Ingener_Technology", ""}};
int rowsWifiList = sizeof(wifiList) / sizeof(wifiList[0]);

// ---------------- Піни ----------------
#define RELE_PIN 4   // D2 (GPIO4) – перевірте відповідність платі
#define POWER_PIN A0 // Аналоговий вхід

// ---------------- Змінні стану ----------------
unsigned long workTime = 0;   // Загальний час роботи (мс)
unsigned long onPcTime = 0;   // Час коли напруга > порогу (мс)
unsigned long lastMillis = 0; // Для інкременту
unsigned long OffPC = 0;      // Час простою
unsigned long OnPC = 0;       // Буфер оновлення onPcTime
int POWER = 0;                // 1=on,0=off
int OnOffPower = 0;           // Для детекції переходів

// ---------------- Параметри логіки ----------------
const float POWER_THRESHOLD = 3.0;         // Вольт (перевірте коефіцієнт дільника!)
const unsigned long SAVE_INTERVAL = 10000; // 10 сек між автозбереженнями
unsigned long lastSave = 0;

ESP8266WebServer server(80);
String deviceName = "Головний комп'ютер";

// ---------------- Прототипи ----------------
void connectWiFi();
void saveData();
void loadData();
void deleteJSON(const char *path);
void handleRoot();
void handleAPIState();
void handleRele();
void handleReset();
void handleClear();
String formatUptime(unsigned long ms);

// ---------------- SETUP ----------------
void setup()
{
  Serial.begin(115200);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, HIGH); // Реле неактивне (залежно від модуля може бути LOW)
  pinMode(LED_BUILTIN, OUTPUT);

  if (!LittleFS.begin())
  {
    Serial.println("[FS] Помилка ініціалізації LittleFS");
  }

  loadData();

  connectWiFi();

  // Початкове вимірювання
  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0f * 5.0f); // Якщо інший дільник – скоригуйте
  POWER = (voltage > POWER_THRESHOLD) ? 1 : 0;
  OnOffPower = POWER;
  if (POWER)
    OnPC = onPcTime;
  else
    OffPC = workTime - OnPC;

  // ---------------- Маршрути ----------------
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleAPIState);
  server.on("/api/rele", HTTP_POST, handleRele);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/clear", HTTP_POST, handleClear);
  server.onNotFound([]()
                    { server.send(404, "text/plain; charset=utf-8", "Not found"); });
  server.begin();
  Serial.println("[HTTP] Server started");
  lastMillis = millis();
}

// ---------------- LOOP ----------------
void loop()
{
  unsigned long now = millis();
  // Акумуляція часу
  workTime += now - lastMillis;
  lastMillis = now;

  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0f * 5.0f);
  if (voltage > POWER_THRESHOLD)
  {
    POWER = 1;
    OnPC = workTime - OffPC;
  }
  else
  {
    POWER = 0;
    OffPC = workTime - OnPC;
  }
  onPcTime = OnPC; // синхронізуємо для збереження

  // Детекція переходів (можна додати логіку повідомлень через інші канали)
  if (OnOffPower != POWER)
  {
    OnOffPower = POWER;
    Serial.println(POWER ? "[STATE] ✅ Ввімкнувся" : "[STATE] ❌ Вимкнувся");
  }

  // Автозбереження
  if (now - lastSave >= SAVE_INTERVAL)
  {
    saveData();
    lastSave = now;
  }

  server.handleClient();

  // Періодичний рестарт (наприклад, 24 години) – зараз ВИМКНЕНО / приклад:
  // if (millis() >= 24UL*60*60*1000UL) ESP.restart();
}

// ---------------- Wi-Fi ----------------
void connectWiFi()
{
  Serial.println("\n[WiFi] Скан списку...");
  for (int i = 0; i < rowsWifiList; i++)
  {
    Serial.print("[WiFi] Підключення до: ");
    Serial.println(wifiList[i][0]);
    WiFi.begin(wifiList[i][0], wifiList[i][1]);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 7000)
    {
      delay(500);
      Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print("[WiFi] ✅ Підключено: ");
      Serial.println(WiFi.SSID());
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
      digitalWrite(LED_BUILTIN, LOW);
      return;
    }
  }
  Serial.println("[WiFi] ❌ Не вдалося підключитись. Рестарт...");
  delay(2000);
  ESP.restart();
}

// ---------------- FS збереження ----------------
void saveData()
{
  StaticJsonDocument<256> doc;
  doc["workTime"] = workTime;
  doc["onPcTime"] = onPcTime;
  File f = LittleFS.open("/data.json", "w");
  if (!f)
  {
    Serial.println("[FS] write error");
    return;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println("[FS] Data saved");
}

void loadData()
{
  if (!LittleFS.exists("/data.json"))
  {
    Serial.println("[FS] data.json не знайдено – створюю");
    saveData();
    return;
  }
  File f = LittleFS.open("/data.json", "r");
  if (!f)
  {
    Serial.println("[FS] read error");
    return;
  }
  StaticJsonDocument<256> doc;
  DeserializationError e = deserializeJson(doc, f);
  if (e)
  {
    Serial.println("[FS] JSON parse error");
    f.close();
    return;
  }
  workTime = doc["workTime"].as<unsigned long>();
  onPcTime = doc["onPcTime"].as<unsigned long>();
  f.close();
  Serial.println("[FS] Data loaded");
}

void deleteJSON(const char *path)
{
  if (LittleFS.exists(path))
  {
    if (LittleFS.remove(path))
      Serial.println("[FS] JSON deleted");
    else
      Serial.println("[FS] JSON delete error");
  }
  else
  {
    Serial.println("[FS] JSON not found");
  }
}

// ---------------- Форматування часу ----------------
String formatUptime(unsigned long ms)
{
  unsigned long s = ms / 1000UL;
  unsigned int sec = s % 60;
  unsigned long m = s / 60UL;
  unsigned int min = m % 60;
  unsigned long h = m / 60UL;
  unsigned int hour = h % 24;
  unsigned long day = h / 24UL;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lu діб %02u:%02u:%02u", day, hour, min, sec);
  return String(buf);
}

// ---------------- HTML Головна ----------------
void handleRoot()
{
  String html = F("<!DOCTYPE html><html lang='uk'><head><meta charset='utf-8'/>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
                  "<title>");
  html += deviceName;
  html += F("</title><style>body{font-family:Arial;margin:14px;background:#111;color:#eee;}"
            "h1{font-size:20px;margin:0 0 10px;}button{padding:10px 18px;margin:4px;font-size:14px;cursor:pointer;border:0;border-radius:6px;background:#3a6aff;color:#fff;}"
            ".stat{margin:8px 0;padding:8px;background:#222;border-radius:6px;line-height:1.4;}"
            "#status.on{color:#6fd96f;}#status.off{color:#ff6b6b;}"
            "code{background:#222;padding:2px 6px;border-radius:4px;}"
            "</style></head><body><h1>");
  html += deviceName;
  html += F(" – панель</h1><div class='stat'>Стан: <span id='status'>...</span><br>Напруга: <span id='voltage'>...</span> V<br>Аптайм пристрою: <span id='uptime'>...</span><br>Час 'ON': <span id='onTime'>...</span><br>Час 'OFF': <span id='offTime'>...</span></div>"
            "<div><button onclick=sendAct('on')>Увімкнути</button><button onclick=sendAct('off')>Вимкнути</button><button onclick=resetDev()>Рестарт</button><button onclick=clearData()>Clear JSON</button></div>"
            "<script>async function load(){const r=await fetch('/api/state');if(!r.ok)return;const j=await r.json();const st=document.getElementById('status');st.textContent=j.power?'ПРАЦЮЄ ✅':'ВИМКНЕНO ❌';st.className=j.power?'on':'off';document.getElementById('voltage').textContent=j.voltage.toFixed(2);document.getElementById('uptime').textContent=j.uptime;document.getElementById('onTime').textContent=j.onTime;document.getElementById('offTime').textContent=j.offTime;}\nfunction sendAct(a){fetch('/api/rele?action='+a,{method:'POST'}).then(()=>setTimeout(load,400));}\nfunction resetDev(){if(confirm('Перезапустити пристрій?'))fetch('/api/reset',{method:'POST'});}\nfunction clearData(){if(confirm('Очистити дані і перезапустити?'))fetch('/api/clear',{method:'POST'});}\nsetInterval(load,2000);load();</script>");
  html += F("</body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

// ---------------- API: стан ----------------
void handleAPIState()
{
  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0f * 5.0f);
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

// ---------------- API: керування реле ----------------
void handleRele()
{
  if (!server.hasArg("action"))
  {
    server.send(400, "text/plain", "Missing action");
    return;
  }
  String a = server.arg("action");
  if (a == "on")
  {
    // Імітуємо кнопку живлення – імпульс 500 мс
    digitalWrite(RELE_PIN, LOW);
    delay(500);
    digitalWrite(RELE_PIN, HIGH);
  }
  else if (a == "off")
  {
    digitalWrite(RELE_PIN, LOW);
    delay(500);
    digitalWrite(RELE_PIN, HIGH);
  }
  else
  {
    server.send(400, "text/plain", "Bad action");
    return;
  }
  server.send(200, "text/plain", "OK");
}

// ---------------- API: reset ----------------
void handleReset()
{
  server.send(200, "text/plain", "Rebooting");
  delay(200);
  ESP.restart();
}

// ---------------- API: clear + reset ----------------
void handleClear()
{
  deleteJSON("/data.json");
  server.send(200, "text/plain", "Cleared");
  delay(300);
  ESP.restart();
}

// ---------------- Кінець ----------------
