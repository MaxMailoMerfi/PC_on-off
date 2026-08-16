// ============================================================
// Arduino Maker Workshop - WEMOS D1 (ESP8266)
// Керування живленням ПК через Telegram бота
// ============================================================
// Інструкція:
// 1. Натисніть [Complite] для створення файлів
// 2. Ctrl+Shift+B -> виберіть [build+copy] для копіювання .bin файлу
// 3. Прошийте WEMOS через програматор
// ============================================================

// ============================================================
// БЛОК 1: КОНФІГУРАЦІЯ ТА КОНСТАНТИ
// ============================================================

// ---- Налаштування Telegram Bot ----
#define BOT_TOKEN "8487247789:AAHBN9wKzSVsCCKi4cJYdYb2Nh83e_itIYE" // Токен бота від @BotFather
#define CHAT_ID_ADMIN "1031379571"                                 // ID адміністратора для сповіщень

// ---- Налаштування апаратних пінів ----
#define relePin 4    // Пін керування реле (D2 на WEMOS)
#define POWER_PIN A0 // Пін вимірювання напруги (A0 - аналоговий вхід)

// ---- Налаштування вимірювання напруги ----
#define VOLTAGE_REF 5.0 // Опорна напруга (залежить від дільника напруги)
#define VOLTAGE_ON 3.0  // Поріг ввімкнення ПК (>3V - ПК працює)
#define VOLTAGE_OFF 2.7 // Поріг вимкнення ПК (<2.7V - ПК вимкнено)
                        // Гістерезис 0.3V для стабільності

// ---- Список Wi-Fi мереж для підключення ----
const char *wifiList[][2] = {
    {"deti_podzemelia", "12345678"}, // Назва мережі, пароль
    {"NewZAG", "12345678"},
    {"Xiaomi 14T", "12345678"}
};

// ============================================================
// БЛОК 2: ГЛОБАЛЬНІ ЗМІННІ
// ============================================================

String myNameBot = "Головний комп'ютер"; // Ім'я бота для відображення

bool pcIsOn = false;      // Поточний стан ПК (true - увімкнено, false - вимкнено)
bool prevPcState = false; // Попередній стан для відправки сповіщень при зміні
int rowsWifiList;         // Кількість Wi-Fi мереж у списку

// ---- Підключення бібліотек ----
#include <FastBot.h>    // Бібліотека для Telegram Bot API
FastBot bot(BOT_TOKEN); // Створення об'єкту бота з токеном

#include <LittleFS.h>    // Файлова система для збереження даних
#include <ArduinoJson.h> // Бібліотека для роботи з JSON

// ---- Змінні для відліку часу ----
unsigned long workTime = 0;     // Загальний час роботи WEMOS (в мілісекундах)
unsigned long onPcTime = 0;     // Час, коли ПК був увімкнений (в мілісекундах)
unsigned long offPcTime = 0;    // Час, коли ПК був вимкнений (в мілісекундах)
unsigned long lastMillis = 0;   // Час останнього оновлення таймерів
unsigned long lastSaveTime = 0; // Час останнього збереження даних

// ============================================================
// БЛОК 3: ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================

/**
 * readVoltage() - Читання напруги з аналогового піну
 * @return float - значення напруги у вольтах
 */
float readVoltage()
{
  int raw = analogRead(POWER_PIN);     // Читаємо сире значення (0-1023)
  return (raw / 1024.0 * VOLTAGE_REF); // Перетворюємо у вольти
}

/**
 * isPcOn() - Визначення стану ПК з гістерезисом
 * @return bool - true якщо ПК увімкнено, false якщо вимкнено
 */
bool isPcOn()
{
  float voltage = readVoltage(); // Отримуємо поточну напругу

  // Гістерезис для уникнення "тремтіння" при близьких значеннях
  if (voltage > VOLTAGE_ON)
  {              // Якщо напруга вище порогу ввімкнення
    return true; // ПК точно увімкнено
  }
  else if (voltage < VOLTAGE_OFF)
  {               // Якщо напруга нижче порогу вимкнення
    return false; // ПК точно вимкнено
  }
  // Якщо напруга між порогами - повертаємо попередній стан (гістерезис)
  return pcIsOn;
}

/**
 * formatTime() - Форматування часу з мілісекунд у читабельний вигляд
 * @param ms - час у мілісекундах
 * @return String - відформатований рядок (напр. "2д 5г 30хв 15с")
 */
String formatTime(unsigned long ms)
{
  unsigned long sec = ms / 1000;    // Переводимо в секунди
  int days = sec / 86400;           // Отримуємо кількість днів
  int hours = (sec % 86400) / 3600; // Отримуємо години
  int minutes = (sec % 3600) / 60;  // Отримуємо хвилини
  int seconds = sec % 60;           // Отримуємо секунди

  String result = "";
  if (days > 0)
    result += String(days) + "д "; // Додаємо дні
  if (hours > 0 || days > 0)
    result += String(hours) + "г ";                          // Додаємо години
  result += String(minutes) + "хв " + String(seconds) + "с"; // Додаємо хвилини та секунди
  return result;
}

// ============================================================
// БЛОК 4: ФУНКЦІЇ РОБОТИ З ФАЙЛАМИ (LittleFS)
// ============================================================

/**
 * saveData() - Збереження даних у JSON файл
 * Зберігає: workTime, onPcTime, offPcTime
 */
void saveData()
{
  StaticJsonDocument<200> doc; // Створюємо JSON документ на 200 байт

  // Заповнюємо JSON даними
  doc["workTime"] = workTime;   // Загальний час роботи
  doc["onPcTime"] = onPcTime;   // Час роботи ПК
  doc["offPcTime"] = offPcTime; // Час вимкнення ПК

  File file = LittleFS.open("/data.json", "w"); // Відкриваємо файл для запису
  if (!file)
  { // Перевіряємо чи відкрито
    Serial.println("❌ Помилка відкриття файлу для запису");
    return;
  }

  serializeJson(doc, file); // Записуємо JSON у файл
  file.close();             // Закриваємо файл
}

/**
 * loadData() - Завантаження даних з JSON файлу
 * Завантажує: workTime, onPcTime, offPcTime
 */
void loadData()
{
  File file = LittleFS.open("/data.json", "r"); // Відкриваємо файл для читання
  if (!file)
  { // Якщо файлу немає
    Serial.println("⚠️ Файл не знайдено, створюємо новий");
    saveData(); // Створюємо новий файл
    return;
  }

  StaticJsonDocument<200> doc;                             // Створюємо JSON документ
  DeserializationError error = deserializeJson(doc, file); // Читаємо JSON

  if (error)
  { // Перевіряємо помилки
    Serial.println("❌ Помилка розбору JSON");
    return;
  }

  // Витягуємо дані з JSON (якщо немає - використовуємо 0)
  workTime = doc["workTime"] | 0;
  onPcTime = doc["onPcTime"] | 0;
  offPcTime = doc["offPcTime"] | 0;

  Serial.println("✅ Дані завантажено!");
  file.close(); // Закриваємо файл
}

// ============================================================
// БЛОК 5: ПІДКЛЮЧЕННЯ ДО WI-FI
// ============================================================

/**
 * connectWiFi() - Підключення до Wi-Fi зі списку мереж
 * Перебирає всі мережі з wifiList до першого успішного підключення
 */
void connectWiFi()
{
  Serial.println("\n🔍 Пошук Wi-Fi\n");

  // Перебираємо всі мережі зі списку
  for (int i = 0; i < 3; i++)
  {
    Serial.print("Пробую підключитися до " + String(wifiList[i][0]));

    WiFi.begin(wifiList[i][0], wifiList[i][1]); // Спроба підключення

    unsigned long startAttemptTime = millis();
    // Чекаємо підключення до 5 секунд
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < 7000)
    {
      delay(200);
      Serial.print("."); // Виводимо крапки під час очікування
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    { // Якщо підключились
      Serial.println("\n✅ Підключено до Wi-Fi: " + String(WiFi.SSID()));
      Serial.println("📡 IP-адреса: " + WiFi.localIP().toString() + "\n");
      return; // Виходимо з функції
    }
  }

  // Якщо жодна мережа не підключилась
  Serial.println("\n❌ Не вдалося підключитися до жодної мережі!");
  delay(1000);
  ESP.restart(); // Перезавантажуємо WEMOS
}

// ============================================================
// БЛОК 6: ОБРОБНИК ПОВІДОМЛЕНЬ TELEGRAM
// ============================================================

/**
 * newMsg() - Обробник вхідних повідомлень від Telegram
 * @param msg - структура з даними повідомлення
 */
void newMsg(FB_msg &msg)
{
  float voltage = readVoltage(); // Отримуємо поточну напругу
  bool pcState = isPcOn();       // Отримуємо стан ПК

  // Логування отриманого повідомлення
  Serial.println("\n========================================");
  Serial.print("📩 Отримано: ");
  Serial.println(msg.text);
  Serial.println("📊 Напруга: " + String(voltage, 2) + "V");
  Serial.println("💻 Стан ПК: " + String(pcState ? "УВІМКНЕНО" : "ВИМКНЕНО"));
  Serial.println("========================================\n");

  String msgText = msg.text;
  String from_name = msg.username;
  if (from_name == "")
    from_name = "Аноним"; // Якщо немає імені

  // ---- OTA оновлення ----
  if (msg.OTA)
  {
    bot.update();
    bot.sendMessage("Загруженно!", msg.chatID);
  }

  // ---- Команда /start - привітання та меню ----
  else if (msgText == "/start")
  {
    bot.tickManual();
    bot.sendMessage("👋 Вітаю, " + from_name + "1!", msg.chatID);
    bot.showMenu("🟢 Увімкнути \t 🔴 Вимкнути \n ⏱ Аптайм \t 📊 Стан \t 🔄 Ресет", msg.chatID);
  }

  // ---- Команда "Увімкнути" - подати живлення на ПК ----
  else if (msgText == "🟢 Увімкнути")
  {
    bot.tickManual();
    if (!pcState)
    {                              // Якщо ПК вимкнено
      digitalWrite(relePin, LOW);  // Замикаємо реле (активний LOW)
      delay(500);                  // Тримаємо 500 мс
      digitalWrite(relePin, HIGH); // Розмикаємо реле
      bot.sendMessage("🔌 Сигнал ввімкнення надіслано!", msg.chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже увімкнено!", msg.chatID);
    }
  }

  // ---- Команда "Вимкнути" - вимкнути живлення ПК ----
  else if (msgText == "🔴 Вимкнути")
  {
    bot.tickManual();
    if (pcState)
    {                             // Якщо ПК увімкнено
      digitalWrite(relePin, LOW); // Замикаємо реле
      delay(500);
      digitalWrite(relePin, HIGH);
      bot.sendMessage("🔌 Сигнал вимкнення надіслано!", msg.chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже вимкнено!", msg.chatID);
    }
  }

  // ---- Команда "Стан" - поточна інформація ----
  else if (msgText == "📊 Стан")
  {
    bot.tickManual();
    String status = pcState ? "✅ працює" : "❌ не працює";
    String response = myNameBot + " " + status + "\n";
    response += "📊 Напруга: " + String(voltage, 2) + "V\n";
    response += "📡 Wi-Fi: " + String(WiFi.SSID()) + "\n";
    response += "📶 Сигнал: " + String(WiFi.RSSI()) + " dBm\n";
    response += "🆔 Chat ID: " + String(msg.chatID);
    bot.sendMessage(response, msg.chatID);
  }

  // ---- Команда "Аптайм" - час роботи ----
  else if (msgText == "⏱ Аптайм")
  {
    bot.tickManual();

    String response = "⏱ **" + myNameBot + "**\n";
    response += "📌 Загальний час: " + formatTime(workTime) + "\n\n";
    response += "🟢 ПК працює: " + formatTime(onPcTime) + "\n";
    response += "🔴 ПК вимкнено: " + formatTime(offPcTime) + "\n";
    // response += "\n📊 Поточний стан: " + String(pcState ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО");

    bot.sendMessage(response, msg.chatID);
    Serial.println(response);
  }

  // ---- Команда "Ресет" - перезавантаження WEMOS ----
  else if (msgText == "🔄 Ресет")
  {
    bot.tickManual();
    bot.sendMessage(myNameBot + ": Перезавантажується.....", msg.chatID);
    delay(500);
    ESP.restart(); // Апаратне перезавантаження
  }

  // ---- Команда "/remove" - видалення даних ----
  else if (msgText == "/remove")
  {
    bot.tickManual();
    if (LittleFS.remove("/data.json"))
    { // Видаляємо JSON файл
      bot.sendMessage("✅ Дані видалено!", msg.chatID);
    }
    delay(500);
    ESP.restart(); // Перезавантажуємо для створення чистого файлу
  }

  // ---- Невідома команда ----
  else if (!msgText.endsWith(".bin"))
  {
    bot.sendMessage("📩 Невідома команда: \"" + msgText + "\"\nНапишіть /start", msg.chatID);
  }
}

// ============================================================
// БЛОК 7: SETUP() - ІНІЦІАЛІЗАЦІЯ
// ============================================================

void setup()
{
  // ---- Ініціалізація Serial ----
  Serial.begin(115200); // Швидкість 115200 бод
  delay(3000);
  Serial.println("\n🚀 Запуск WEMOS...");
  delay(3000);

  // ---- Ініціалізація LittleFS (файлова система) ----
  if (!LittleFS.begin())
  {
    Serial.println("⚠️ Помилка LittleFS! Форматую...");
    LittleFS.format(); // Форматуємо файлову систему при помилці
    if (LittleFS.begin())
    {
      Serial.println("✅ LittleFS готовий!");
    }
  }
  else
  {
    Serial.println("✅ LittleFS готовий!");
  }

  // ---- Налаштування пінів ----
  pinMode(POWER_PIN, INPUT);   // Пін вимірювання напруги - вхід
  pinMode(relePin, OUTPUT);    // Пін реле - вихід
  digitalWrite(relePin, HIGH); // Реле вимкнено (нормально-розімкнуте)

  // ---- Завантаження збережених даних ----
  loadData();

  // ---- Визначення початкового стану ПК ----
  float voltage = readVoltage();
  pcIsOn = (voltage > VOLTAGE_ON); // Визначаємо стан за напругою
  prevPcState = pcIsOn;            // Запам'ятовуємо для відправки сповіщень

  Serial.println("📊 Початкова напруга: " + String(voltage, 2) + "V");
  Serial.println("💻 Стан ПК: " + String(pcIsOn ? "УВІМКНЕНО" : "ВИМКНЕНО"));

  // ---- Підключення до Wi-Fi ----
  rowsWifiList = (sizeof(wifiList) / sizeof(wifiList[0])); // Розмір масиву
  connectWiFi();

  // ---- Ініціалізація Telegram бота ----
  bot.attach(newMsg); // Підключаємо обробник повідомлень
  delay(1000);

  // ---- Відправка привітального повідомлення ----
  Serial.println("\n✅ WEMOS: увімкнувся!");
  String startMsg = "✅ WEMOS: увімкнувся!\n";
  startMsg += "💻 ПК: " + String(pcIsOn ? "✅ працює" : "❌ не працює");
  startMsg += "\n📊 Напруга: " + String(voltage, 2) + "V";
  bot.sendMessage(startMsg, CHAT_ID_ADMIN);

  // ---- Ініціалізація таймерів ----
  lastMillis = millis();   // Запам'ятовуємо початковий час
  lastSaveTime = millis(); // Для автоматичного збереження
}

// ============================================================
// БЛОК 8: LOOP() - ГОЛОВНИЙ ЦИКЛ
// ============================================================

void loop()
{
  // ---- Контроль Wi-Fi підключення ----
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠️ Wi-Fi відключено! Перепідключаюсь...");
    connectWiFi(); // Спроба перепідключення
  }

  bot.tick(); // Обробка вхідних повідомлень Telegram

  // ---- Оновлення таймерів часу ----
  unsigned long currentMillis = millis();
  unsigned long delta = currentMillis - lastMillis; // Час з останнього циклу

  if (delta > 0)
  {
    workTime += delta; // Загальний час роботи WEMOS

    // Розподіляємо час залежно від стану ПК
    if (pcIsOn)
    {
      onPcTime += delta; // Час роботи ПК
    }
    else
    {
      offPcTime += delta; // Час вимкнення ПК
    }
  }
  lastMillis = currentMillis;

  // ---- Вимірювання напруги та визначення стану ПК ----
  float voltage = readVoltage();
  bool newPcState = isPcOn();

  // Логування стану (кожні 10 секунд або при зміні)
  static unsigned long lastPrint = 0;
  if (newPcState != pcIsOn || currentMillis - lastPrint > 10000)
  {
    lastPrint = currentMillis;
    Serial.print("📊 Напруга: " + String(voltage, 2) + "V");
    Serial.println(" | PC: " + String(newPcState ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО"));
  }

  // ---- Відправка сповіщень при зміні стану ПК ----
  if (newPcState != pcIsOn)
  {
    pcIsOn = newPcState; // Оновлюємо стан

    if (pcIsOn)
    {
      Serial.println("💻 ПК УВІМКНУВСЯ!");
      bot.sendMessage("✅ ПК ввімкнувся!\n📊 Напруга: " + String(voltage, 2) + "V", CHAT_ID_ADMIN);
    }
    else
    {
      Serial.println("💻 ПК ВИМКНУВСЯ!");
      bot.sendMessage("❌ ПК вимкнувся!\n📊 Напруга: " + String(voltage, 2) + "V", CHAT_ID_ADMIN);
    }
  }

  // ---- Автоматичне збереження даних (кожну секунду) ----
  if (currentMillis - lastSaveTime > 1000)
  {
    lastSaveTime = currentMillis;
    saveData(); // Зберігаємо дані у файл
  }

  // ---- Планове перезавантаження кожні 24 години ----
  static unsigned long lastRestart = 0;
  if (currentMillis - lastRestart >= 86400000UL)
  { // 24 години в мілісекундах
    lastRestart = currentMillis;
    Serial.println("🔄 Перезавантаження через 24 години...");
    saveData(); // Зберігаємо дані перед перезавантаженням
    bot.sendMessage("🔄 Планове перезавантаження...", CHAT_ID_ADMIN);
    delay(1000);
    ESP.restart(); // Перезавантажуємо WEMOS
  }

  delay(10); // Невелика затримка для стабільності
}