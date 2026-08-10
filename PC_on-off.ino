// Arduino Maker Workshop
// Щоб створити файли для праці і обновити [bin], натисніть [Complite] внизу праворуч
// файл bin копіюється натисканням [Ctrl+Shift+B] та вибором [build+copy] і знаходиться поруч з ino

const char *wifiTest[][2] = {
    {"deti_podzemelia", "12345678"},
};

const char *wifiList[][2] = {
    {"deti_podzemelia", "12345678"},
    {"Xiaomi 14T", "12345678"},
    {"Ingener_Technology", ""}};

#define BOT_TOKEN "8487247789:AAHBN9wKzSVsCCKi4cJYdYb2Nh83e_itIYE"
#define relePin 4
#define POWER_PIN A0
#define CHAT_ID_ADMIN "1031379571"

String myNameBot = "Головний комп'ютер";
int POWER, OnPC = 0, OffPC = 0, OnOffPower, rowsWifiList;
String PC = "";

#include <FastBot.h>
FastBot bot(BOT_TOKEN);
#include <LittleFS.h>
#include <ArduinoJson.h>

unsigned long workTime = 0, onPcTime = 0, lastMillis = 0;
unsigned long lastSaveTime = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // ===== ВИПРАВЛЕННЯ LITTLEFS =====
  if (!LittleFS.begin())
  {
    Serial.println("⚠️ Помилка LittleFS! Форматую...");
    LittleFS.format();
    if (LittleFS.begin())
    {
      Serial.println("✅ LittleFS готовий!");
    }
  }
  else
  {
    Serial.println("✅ LittleFS готовий!");
  }

  pinMode(POWER_PIN, INPUT);

  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0 * 5.0);
  if (voltage > 3.0)
  {
    POWER = 1;
  }
  else
  {
    POWER = 0;
  }
  OnOffPower = POWER;

  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, 1);

  loadData();

  rowsWifiList = (sizeof(wifiList) / sizeof(wifiList[0]));

  OnPC = onPcTime;
  OffPC = workTime - OnPC;

  delay(1000);
  connectWiFi();

  bot.attach(newMsg);
  delay(1000);

  Serial.println("\n✅ WEMOS: увімкнувся!");
  bot.sendMessage("✅ WEMOS: увімкнувся! \n/start", CHAT_ID_ADMIN);

  lastMillis = millis();
  lastSaveTime = millis();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠️ Wi-Fi відключено! Перепідключаюсь...");
    connectWiFi();
  }

  bot.tick();

  unsigned long currentMillis = millis();
  workTime += currentMillis - lastMillis;
  lastMillis = currentMillis;

  if (currentMillis - lastSaveTime > 1000)
  {
    lastSaveTime = currentMillis;
    saveData();
  }

  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0 * 5.0);

  if (voltage > 3)
  {
    POWER = 1;
    OnPC = workTime - OffPC;
  }
  else if (voltage < 3)
  {
    POWER = 0;
    OffPC = workTime - OnPC;
  }

  if (OnOffPower < POWER)
  {
    OnOffPower = POWER;
    bot.sendMessage("✅ ПК ввімкнувся", CHAT_ID_ADMIN);
  }
  else if (OnOffPower > POWER)
  {
    OnOffPower = POWER;
    bot.sendMessage("❌ ПК вимкнувся", CHAT_ID_ADMIN);
  }

  if (millis() / 1000 / 60 / 60 >= 24)
  {
    Serial.println("🔄 Перезавантаження через 24 години...");
    ESP.restart();
  }

  delay(10);
}

void newMsg(FB_msg &msg)
{
  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0 * 6.0);

  Serial.println("\n========================================");
  Serial.print("📩 Отримано: ");
  Serial.println(msg.text);
  Serial.println("========================================\n");

  String msgText = msg.text;
  String from_name = msg.username;
  if (from_name == "")
    from_name = "Аноним";

  if (msg.OTA && msg.text == "ADMIN")
    bot.update();

  if (msgText == "/start")
  {
    bot.tickManual();
    bot.sendMessage("👋 Вітаю, " + from_name + "!", msg.chatID);
    bot.showMenu(textMenu("start"), msg.chatID);
  }
  else if (msgText == "Увімкнути")
  {
    bot.tickManual();
    if (voltage < 4.0)
    {
      digitalWrite(relePin, 0);
      delay(500);
      digitalWrite(relePin, 1);
      bot.sendMessage("🔌 ПК вмикається...", msg.chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже увімкнено!", msg.chatID);
    }
  }
  else if (msgText == "Вимкнути")
  {
    bot.tickManual();
    if (voltage > 4.0)
    {
      digitalWrite(relePin, 0);
      delay(500);
      digitalWrite(relePin, 1);
      bot.sendMessage("🔌 ПК вимикається...", msg.chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже вимкнено!", msg.chatID);
    }
  }
  else if (msgText == "Стан")
  {
    bot.tickManual();
    String status = (voltage > 4.0) ? "✅ працює" : "❌ не працює";
    bot.sendMessage(
        myNameBot + " " + status + "\n"
                                   "📊 Напруга: " +
            String(voltage) + "V\n"
                              "📡 Wi-Fi: " +
            String(WiFi.SSID()) + "\n"
                                  "🆔 Chat ID: " +
            String(msg.chatID),
        msg.chatID);
  }
  else if (msgText == "Аптайм")
  {
    bot.tickManual();

    // ===== ВАШ ОРИГІНАЛЬНИЙ АПТАЙМ =====
    unsigned long myTime = workTime;
    int mymin = myTime / 1000 / 60;
    int myhour = mymin / 60;
    int myday = myhour / 24;

    String msg0 = String((myTime / 1000) % 60);
    String msg1 = String(mymin % 60);
    String msg2 = String(myhour % 24);
    String msg3 = String(myday);
    String msgs = String(myNameBot + "- WEMOS: працює \n" + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин " + msg0 + " секунд");

    myTime = OnPC;
    mymin = myTime / 1000 / 60;
    myhour = mymin / 60;
    myday = myhour / 24;
    msg0 = String((myTime / 1000) % 60);
    msg1 = String(mymin % 60);
    msg2 = String(myhour % 24);
    msg3 = String(myday);
    String msgOnPC = String(myNameBot + ": працює \n" + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин " + msg0 + " секунд");

    myTime = OffPC;
    mymin = myTime / 1000 / 60;
    myhour = mymin / 60;
    myday = myhour / 24;
    msg0 = String((myTime / 1000) % 60);
    msg1 = String(mymin % 60);
    msg2 = String(myhour % 24);
    msg3 = String(myday);
    String msgOffPC = String(myNameBot + ": в очікувані \n" + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин " + msg0 + " секунд");

    bot.sendMessage(msgs + "\n\n" + msgOnPC + "\n\n" + msgOffPC + "\n\n", msg.chatID);
    Serial.println(msgs + "\n\n" + msgOnPC + "\n\n" + msgOffPC + "\n\n");
  }
  else if (msgText == "Ресет")
  {
    bot.tickManual();
    bot.sendMessage(myNameBot + ": Перезавантажується.....", msg.chatID);
    delay(500);
    ESP.restart();
  }
  else if (msgText == "/remove")
  {
    String jsonFile = "data";
    bot.tickManual();
    deleteJSON("/" + jsonFile + ".json");
    ESP.restart();
  }
  else
  {
    bot.sendMessage("📩 Невідома команда: \"" + msgText + "\"\nНапишіть /start", msg.chatID);
  }
}

void connectWiFi()
{
  Serial.println("\n🔍 Пошук Wi-Fi\n");

  for (int i = 0; i < rowsWifiList - 1; i++)
  {
    Serial.println("Пробую підключитися до " + String(wifiList[i][0]) + " | " + myNameBot);
    WiFi.begin(wifiList[i][0], wifiList[i][1]);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < 5000)
    {
      delay(200);
    }

    digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED ? LOW : HIGH);

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\n✅ Підключено до Wi-Fi: " + String(WiFi.SSID()) + " | " + myNameBot);
      Serial.println("📡 IP-адреса: " + WiFi.localIP().toString() + "\n");
      return;
    }
  }

  Serial.println("\n❌ Не вдалося підключитися до жодної мережі!");
  delay(1000);
  ESP.restart();
}

void saveData()
{
  StaticJsonDocument<200> doc;
  doc["workTime"] = workTime;
  doc["onPcTime"] = onPcTime;

  File file = LittleFS.open("/data.json", "w");
  if (!file)
  {
    Serial.println("❌ Помилка відкриття файлу для запису");
    return;
  }

  serializeJson(doc, file);
  file.close();
  Serial.println("✅ Дані збережено!");
}

void loadData()
{
  File file = LittleFS.open("/data.json", "r");
  if (!file)
  {
    Serial.println("⚠️ Файл не знайдено, створюємо новий");
    saveData();
    return;
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.println("❌ Помилка розбору JSON");
    return;
  }

  workTime = doc["workTime"] | 0;
  onPcTime = doc["onPcTime"] | 0;
  Serial.println("✅ Дані завантажено!");
  file.close();
}

void deleteJSON(String jsonFile)
{
  if (LittleFS.exists(jsonFile))
  {
    if (LittleFS.remove(jsonFile))
    {
      Serial.println("✅ JSON-файл видалений!");
      bot.sendMessage("✅ JSON-файл видалений!", CHAT_ID_ADMIN);
    }
  }
}

String textMenu(String menu)
{
  if (menu == "start")
  {
    return "Увімкнути \t Вимкнути \n Аптайм \t Стан \t Ресет";
  }
  return "невідома команда";
}