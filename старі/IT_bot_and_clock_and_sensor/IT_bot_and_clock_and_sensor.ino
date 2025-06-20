#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <FastBot.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// --- Налаштування WiFi ---
#define WIFI_SSID "Ingener_Technology"
#define WIFI_PASS ""

// --- Telegram Bot ---
#define BOT_TOKEN   "7715526853:AAGLMLpdxnjXrhzffs5zQfPe0mABqOR_CKE"
#define CHAT_ID_1   "-1002404611795"
#define CHAT_ID_2   "1031379571"
FastBot bot(BOT_TOKEN);
const String botVersion = " Версія 5.1";
const String myNameBot = "BDUTGyverPlafon";

// --- Таймер ---
#define LONG_INTERVAL   (5UL * 60 * 1000)
#define SHORT_INTERVAL  (20UL * 1000)
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
unsigned long interval = SHORT_INTERVAL;

// --- Стан ---
bool releStatus = false;
bool wasConnected = false;

// --- Ключові фрази ---
const char* ALERT_TEXT  = "Повітряна тривога!!";
const char* CANCEL_TEXT = "ВІДБІЙ повітряної тривоги";

// --- Клімат ---
String sensorBMP = "Connect";
float temperature = 0;
int32_t pressure = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi не підключено. Перезапуск...");
    clearMatrix();
    ESP.restart();
  }
  wasConnected = true;
  clockInit();
  sendStartMessage();
  bot.attach(newMsg);
}

void loop() {
  currentMillis = millis();
  checkWiFi();
  clockTick();
  handleRele();

  // Перезавантаження через 3 доби
  if (currentMillis > 259200000UL && !releStatus) {
    sendMessageAll(myNameBot + ": працюю три доби. Перезавантажуюсь...");
    clearMatrix();
    ESP.restart();
  }
}

void checkWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (wasConnected)
    {
      Serial.println("WiFi втрачено. Переходжу в offline...");
      wasConnected = false;
    }
  }
  else 
  {
    if (!wasConnected) 
    {
      Serial.println("WiFi знову з’явився. Перезапуск...");
      clearMatrix();
      ESP.restart();
    }
    wasConnected = true;
    bot.tick();
  }
}

void handleRele() {
  if (releStatus && (currentMillis - previousMillis >= interval)) {
    releStatus = false;
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void connectWiFi() {
  Serial.println("Підключення до WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi підключено!" : "\nWiFi не вдалося підключити.");
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED ? LOW : HIGH);
}

void sendStartMessage() {
  String msg = myNameBot + " увімкнувся!" + botVersion;
  sendMessageAll(msg);
}

void sendMessageAll(const String& msg) {
  bot.sendMessage(msg, CHAT_ID_1);
  // bot.sendMessage(msg, CHAT_ID_2);
}

void newMsg(FB_msg& msg) {
  String text = msg.text;
  String from = msg.username == "" ? "Анонім" : msg.username;
  uint32_t unixNow = bot.getUnix();

  if (msg.OTA && text == "IT") {
    bot.update();
    return;
  }

  if (text.indexOf(ALERT_TEXT) != -1) {
    interval = LONG_INTERVAL;
    sendMessageAll(myNameBot + ": Отримано повідомлення про тривогу!");
    digitalWrite(LED_BUILTIN, LOW);
    releStatus = true;
    previousMillis = millis();
    return;
  }

  if (text.indexOf(CANCEL_TEXT) != -1) {
    if (unixNow - msg.unix > 30) {
      sendMessageAll(myNameBot + ": Повідомлення про відбій застаріле!");
    } else {
      interval = SHORT_INTERVAL;
      sendMessageAll(myNameBot + ": Повідомлення про відбій актуальне! Вмикаю відбій.");
      digitalWrite(LED_BUILTIN, LOW);
      releStatus = true;
      previousMillis = millis();
    }
    return;
  }

  if (text == "Увімкнути сирену") {
    interval = LONG_INTERVAL;
    releStatus = true;
    previousMillis = millis();
    bot.sendMessage("Сирена увімкнена", msg.chatID);
    return;
  }

  if (text == "Вимкнути сирену") {
    releStatus = false;
    digitalWrite(LED_BUILTIN, HIGH);
    bot.sendMessage("Сирена вимкнена", msg.chatID);
    return;
  }

  if (text == "Стан сирени") {
    String status = releStatus ? "Сирена виє." : "Сирена не виє.";
    bot.sendMessage(myNameBot + ": " + status, msg.chatID);
    return;
  }

  if (text == "Аптайм") {
    unsigned long uptime = millis();
    int min = uptime / 60000UL;
    int hour = min / 60;
    int day = hour / 24;
    String msgText = myNameBot + " працює " + day + " діб " + (hour % 24) + " год " + (min % 60) + " хв";
    bot.sendMessage(msgText, msg.chatID);
    return;
  }

  if (text == "/start") {
    bot.sendMessage("Вітаю, " + from + ".\n" + msg.chatID, msg.chatID);
    bot.showMenu("Клімат контроль\tАптайм\nУвімкнути сирену\tСтан сирени\tВимкнути сирену", msg.chatID);
    return;
  }

  if (text == "Клімат контроль") {
    sensor();
    bot.sendMessage("Температура: " + String(temperature) + " °C", msg.chatID);
    bot.sendMessage("Тиск: " + String(pressure / 100) + " гПа", msg.chatID);
    bot.sendMessage("Або: " + String(pressure / 133.322) + " мм рт. ст.", msg.chatID);
    return;
  }

  if (text == "Ресет") {
    bot.tickManual();
    bot.sendMessage(myNameBot + ": Перезавантажуюсь...", msg.chatID);
    clearMatrix();
    ESP.restart();
  }
}
