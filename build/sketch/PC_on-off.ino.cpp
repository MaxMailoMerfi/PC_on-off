#include <Arduino.h>
#line 1 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
//Arduino Maker Workshop
//Щоб створити файли для праці і обновити [bin], натисніть [Complite] внизу праворуч
//файл bin копіюється натисканням [Ctrl+Shift+B] та вибором [build+copy] і знаходиться поруч з ino

const char* wifiTest[][2] = {
  {"deti_podzemelia", "12345678"},
};

const char* wifiList[][2] = {
  {"deti_podzemelia", "12345678"},
  {"Xiaomi 14T", ""},
  {"Ingener_Technology", ""}
};

#define BOT_TOKEN "7714508177:AAEt_BhjKln3t6FgsjBMV5biA4iMw6zKw-E" // 
#define relePin   4 // номер контакту для підключення реле
#define POWER_PIN A0
#define CHAT_ID_ADMIN "1031379571"
String myNameBot = "Головний комп'ютер";
int POWER, OnPC = 0, OffPC = 0, OnOffPower,rowsWifiList;
  String PC = "";

#include <FastBot.h>                      //https://github.com/GyverLibs/FastBot
FastBot bot(BOT_TOKEN);
#include <LittleFS.h>
#include <ArduinoJson.h>
unsigned long workTime = 0,onPcTime = 0, lastMillis = 0, lastOnPC = 0;



#line 31 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void setup();
#line 75 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void loop();
#line 117 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void newMsg(FB_msg& msg);
#line 257 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void connectWiFi();
#line 290 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void saveData();
#line 310 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void loadData();
#line 335 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void deleteJSON(String jsonFile);
#line 357 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
String textMenu(String menu);
#line 31 "D:\\Max\\WEMOS\\PC_on-off\\PC_on-off.ino"
void setup()
{
  Serial.begin(115200);
  pinMode(POWER_PIN, INPUT);
  
  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0 * 5.0);
  if (voltage > 3.0)
  {
    POWER = 1;//on
  }
  else
  {
    POWER = 0;//off
  }
  OnOffPower = POWER;

  pinMode(relePin, OUTPUT);               // налаштовуємо контакт для реле як вихід
  digitalWrite(relePin, 1);            // вимикаємо реле
  
  loadData();  // Завантажуємо дані

  rowsWifiList = (sizeof(wifiList) / sizeof(wifiList[0]));

  OnPC = onPcTime;
  OffPC = workTime - OnPC;
  
  delay(1000);
  connectWiFi();                          // підключаємося до мережі
  
  
  if (!LittleFS.begin()) {
    Serial.println("❌ Помилка ініціалізації LittleFS");
    bot.sendMessage("❌ Помилка ініціалізації LittleFS", CHAT_ID_ADMIN);
    return;
  }

  Serial.println("\nWEMOS: увімкнувся!");
  // bot.sendMessage("WEMOS: увімкнувся! \n/start", CHAT_ID_ADMIN);

  bot.attach(newMsg);                     // підключаємо функцію - обробник повідомлень
}

// основний цикл
void loop()
{

  workTime += millis()-lastMillis;  // Додаємо час роботи
  onPcTime = OnPC;  // Оновлюємо температуру
  lastMillis = millis();
  saveData();  // Зберігаємо нові дані
  bot.tick();
  
  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0 * 5.0);

  if(voltage > 3)
  {
    POWER = 1;//on
    OnPC = workTime - OffPC;
  }
  else if(voltage < 3)
  {
    POWER = 0;//off
    OffPC = workTime - OnPC;
  }

  if (OnOffPower < POWER)
  {
    OnOffPower = POWER;
    bot.sendMessage("✅Ввімкнувся", CHAT_ID_ADMIN);
  }
  else if (OnOffPower > POWER)
  {
    OnOffPower = POWER;
    bot.sendMessage("❌Вимкнувся", CHAT_ID_ADMIN);
  }

  //якщо минуло 1 доби, то перезавантажимося
  if (millis() / 1000 / 60 / 60 >= 1  /*&& voltage < 4.0*/)
  {
    // bot.sendMessage(myNameBot + ": . Перевантажуюсь...", CHAT_ID_ADMIN);
    ESP.restart();
  }
}
// Обробник повідомлень
void newMsg(FB_msg& msg)
{
  int raw = analogRead(POWER_PIN);
  float voltage = (raw / 1024.0 * 6.0);

  Serial.println("\n\n\n" + msg.text + "\n\n\n");

  uint32_t curentTime, messageTime;
  String msgText = msg.text;              // із структури повідомлення виділяємо сам текст
  String from_name = msg.username;        // із структури повідомлення виділяємо хто надіслав
  if (from_name == "")
    from_name = "Аноним";

  // int32_t msgID = msg.messageID;  // ID сообщения

  curentTime = bot.getUnix();     // поточний час в ЮНІКС форматі
  messageTime = msg.unix - 2;     // зменшимо час повідомлення щоб уникнути глюк



// обновить, если файл имеет нужную подпись
  if (msg.OTA && msg.text == "ADMIN")bot.update();

// обробляємо натискання софт-кнопки в юзер меню в месенджері

  if (msgText == "/start")// формуємо юзер меню з софт кнопками
  {
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    //String welcome = "Вітаю, " + from_name + ".\n";
    bot.sendMessage("Вітаю, " + from_name + ".", msg.chatID);
    Serial.println("\nВітаю, " + from_name + ".\n");

    // показати юзер меню (\t - горизонтальний поділ кнопок, \n - вертикальний
    bot.showMenu(textMenu("start"), msg.chatID);
  }

  

  if (msgText == "Увімкнути")
  {
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    if (voltage < 4.0)
    {
      digitalWrite(relePin, 0);             // вмикаємо реле
      delay(500);
      digitalWrite(relePin, 1);
    }
    else
    {
      bot.sendMessage("Увімкнений", msg.chatID);
    }
  }

  if (msgText == "Вимкнути")
  {
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    if (voltage > 4.0)
    {
      digitalWrite(relePin, 0);            // вимикаємо реле
      delay(500);
      digitalWrite(relePin, 1);
    }
    else
    {
      bot.sendMessage("Вимкнений", msg.chatID);
    }
  }

  if (msgText == "Стан")
  {
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    if (voltage > 4.0)
      {
        PC = ": працює.✅";
      }
    else
      {
        PC = ": не працює.❌";
      }
    bot.sendMessage(myNameBot + PC + "\nChat ID: " + String(msg.chatID) + "\nAnalog: " + String(voltage) + "\n" + rowsWifiList, msg.chatID);
    bot.sendMessage("Підключено до Wi-Fi: " + String(WiFi.SSID()) + " | " + myNameBot, CHAT_ID_ADMIN);
    Serial.println(myNameBot + PC + "\nChat ID: " + String(msg.chatID) + "\nAnalog: " + String(voltage) + "\n" + rowsWifiList + "\n");
  }

  if (msgText == "Аптайм")
  {
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    unsigned long myTime = workTime;
    int mymin = myTime / 1000 / 60;
    int myhour = mymin / 60;
    int myday = myhour / 24;

    String msg0 = String((myTime /1000) % 60);
    String msg1 = String(mymin % 60);
    String msg2 = String(myhour % 24);
    String msg3 = String(myday);
    String msgs = String(myNameBot + "- WEMOS: працює \n" + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин " + msg0 + " секунд");
    
    myTime = OnPC;
    mymin = myTime / 1000 / 60;
    myhour = mymin / 60;
    myday = myhour / 24;
    msg0 = String((myTime /1000) % 60);
    msg1 = String(mymin % 60);
    msg2 = String(myhour % 24);
    msg3 = String(myday);
    String msgOnPC = String(myNameBot + ": працює \n" + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин " + msg0 + " секунд");
    
    myTime = OffPC;
    mymin = myTime / 1000 / 60;
    myhour = mymin / 60;
    myday = myhour / 24;
    msg0 = String((myTime /1000) % 60);
    msg1 = String(mymin % 60);
    msg2 = String(myhour % 24);
    msg3 = String(myday);
    String msgOffPC = String(myNameBot + ": в очікувані \n" + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин " + msg0 + " секунд");

    bot.sendMessage(msgs + "\n\n" + msgOnPC + "\n\n" + msgOffPC + "\n\n", msg.chatID);
    Serial.println(msgs + "\n\n" + msgOnPC + "\n\n" + msgOffPC + "\n\n");
  }

  if (msgText == "Ресет")
  {
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    bot.sendMessage(myNameBot +": Перезавантажується.....", msg.chatID);
    Serial.println("\n" + myNameBot +": Перезавантажується.....\n");
    ESP.restart();
  }

  if (msgText == "/remove")
  {
    String jsonFile = "data";
    bot.tickManual();  // Скидаємо кеш отриманих повідомлень
    deleteJSON("/" + jsonFile + ".json");  // Видаляємо JSON-файл
    ESP.restart();
  }
  
}

void connectWiFi()
{

  Serial.println("\n🔍 Пошук Wi-Fi\n");

  unsigned long startAttemptTime;  // Початковий час підключення

  for (int i = 0; i < rowsWifiList - 1; i++) {
    Serial.println("Пробую підключитися до " + String(wifiList[i][0]) + " | " + myNameBot);
    WiFi.begin(wifiList[i][0], wifiList[i][1]);

    startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < 5000) {
      delay(500);
    }
    Serial.println(String(millis() - startAttemptTime));
    
    digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED ? LOW : HIGH);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Підключено до Wi-Fi: " + String(WiFi.SSID()) + " | " + myNameBot);
      Serial.println("📡 IP-адреса: " + WiFi.localIP().toString() + "\n");
      break;  // Вийти з циклу, якщо підключилися
    }
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("\n❌ Не вдалося підключитися до жодної мережі.\n Перезавантажую... | " + myNameBot);
    ESP.restart();
  }
}

// Функція збереження JSON у файл
void saveData() {
  StaticJsonDocument<200> doc;
  doc["workTime"] = workTime;
  doc["onPcTime"] = onPcTime;
  // doc["lastMillis"] = lastMillis;

  File file = LittleFS.open("/data.json", "w");
  if (!file) {
    Serial.println("❌ Помилка відкриття файлу для запису (відсутній)");
    bot.sendMessage("❌ Помилка відкриття файлу для запису (відсутній)", CHAT_ID_ADMIN);
    return;
  }

  serializeJson(doc, file);  // Запис JSON у файл 
  file.close();
  Serial.println("✅ Дані збережено у JSON!");
}


// Функція зчитування JSON з файлу
void loadData() {
  File file = LittleFS.open("/data.json", "r");
  if (!file) {
    Serial.println("⚠️ Файл не знайдено, створюємо новий");
    bot.sendMessage("⚠️ Файл не знайдено, створюємо новий", CHAT_ID_ADMIN);
    saveData();  // Створюємо файл із початковими значеннями
    return;
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("❌ Помилка розбору JSON");
    bot.sendMessage("❌ Помилка розбору JSON", CHAT_ID_ADMIN);
    return;
  }

  workTime = doc["workTime"];
  onPcTime = doc["onPcTime"];
  // lastMillis = doc["lastMillis"];
  
  Serial.println("✅ Дані завантажено з JSON!");
  file.close();
}

void deleteJSON(String jsonFile)
{
  
  if (LittleFS.exists(jsonFile)) {  // Перевіряємо, чи існує файл
    if (LittleFS.remove(jsonFile)) 
    {  // Видаляємо файл
      Serial.println("✅ JSON-файл успішно видалений!");
      bot.sendMessage("✅ JSON-файл успішно видалений!", CHAT_ID_ADMIN);
    }
    else
    {
      Serial.println("❌ Помилка видалення JSON-файлу!");
      bot.sendMessage("❌ Помилка видалення JSON-файлу!", CHAT_ID_ADMIN);
    }
  }
  else 
  {
    Serial.println("⚠️ Файл JSON не знайдено!");
    bot.sendMessage("⚠️ Файл JSON не знайдено!", CHAT_ID_ADMIN);
  }
}

String textMenu(String menu)
{
  String text;
  if (menu == "start"){text = "Увімкнути \t  Вимкнути \n Аптайм \t Стан \t Ресет";}
  else if (menu == "wifi"){text = "Переглянути Log";}
  
  else {text = "невідома команда для меню";}

  return text;
}
