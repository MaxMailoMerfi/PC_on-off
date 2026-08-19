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

// ---- Налаштування NTP (синхронізація часу) ----
#define NTP_SERVER "pool.ntp.org"   // NTP сервер
#define TIMEZONE_OFFSET 3           // Часовий пояс (UTC+3 для Києва)
#define NTP_UPDATE_INTERVAL 3600000 // Оновлення часу кожну годину (в мілісекундах)

// ---- Список Wi-Fi мереж для підключення ----
const char *wifiList[][2] = {
    {"deti_podzemelia", "12345678"}, // Назва мережі, пароль
    {"NewZAG", "12345678"},
    {"Xiaomi 14T", "12345678"}};

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
#include <time.h>        // Бібліотека для роботи з часом

// ---- Змінні для відліку часу ----
unsigned long workTime = 0;      // Загальний час роботи WEMOS (в мілісекундах)
unsigned long onPcTime = 0;      // Час, коли ПК був увімкнений (в мілісекундах)
unsigned long offPcTime = 0;     // Час, коли ПК був вимкнений (в мілісекундах)
unsigned long lastMillis = 0;    // Час останнього оновлення таймерів
unsigned long lastSaveTime = 0;  // Час останнього збереження даних
unsigned long lastNtpUpdate = 0; // Час останнього оновлення NTP

// ---- Змінні для історії подій ----
struct EventLog
{
  time_t timestamp; // Час події (Unix timestamp)
  bool state;       // Стан ПК (true - ввімкнено, false - вимкнено)
  float voltage;    // Напруга в момент події
};

#define MAX_EVENTS 1000 // Максимальна кількість подій для збереження
EventLog eventLog[MAX_EVENTS];
int eventCount = 0;

// ---- Змінні для поточного часу ----
time_t currentTime = 0;
struct tm timeinfo;

// ---- Змінна для відстеження стану меню ----
bool inMonthMenu = false;

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

/**
 * getMonthName() - Отримання назви місяця
 * @param month - номер місяця (1-12)
 * @return String - назва місяця українською
 */
String getMonthName(int month)
{
  const char *months[] = {"Січень", "Лютий", "Березень", "Квітень", "Травень", "Червень",
                          "Липень", "Серпень", "Вересень", "Жовтень", "Листопад", "Грудень"};
  if (month >= 1 && month <= 12)
  {
    return String(months[month - 1]);
  }
  return "Невідомо";
}

/**
 * getDayName() - Отримання назви дня тижня
 * @param day - номер дня (0-6, де 0 - неділя)
 * @return String - назва дня українською
 */
String getDayName(int day)
{
  const char *days[] = {"Неділя", "Понеділок", "Вівторок", "Середа",
                        "Четвер", "П'ятниця", "Субота"};
  if (day >= 0 && day <= 6)
  {
    return String(days[day]);
  }
  return "Невідомо";
}

/**
 * formatDateTime() - Форматування дати та часу з Unix timestamp
 * @param timestamp - Unix timestamp
 * @return String - відформатований рядок (напр. "01.01.2024 12:30:15")
 */
String formatDateTime(time_t timestamp)
{
  struct tm *tm_info = localtime(&timestamp);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", tm_info);
  return String(buffer);
}

/**
 * formatDateTimeShort() - Коротке форматування дати та часу
 * @param timestamp - Unix timestamp
 * @return String - відформатований рядок (напр. "01.01 12:30")
 */
String formatDateTimeShort(time_t timestamp)
{
  struct tm *tm_info = localtime(&timestamp);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d.%m %H:%M", tm_info);
  return String(buffer);
}

/**
 * getCurrentMonth() - Отримання поточного місяця
 * @return int - номер місяця (1-12)
 */
int getCurrentMonth()
{
  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);
  return tm_info->tm_mon + 1;
}

/**
 * getCurrentYear() - Отримання поточного року
 * @return int - рік
 */
int getCurrentYear()
{
  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);
  return tm_info->tm_year + 1900;
}

/**
 * getMonthDays() - Отримання кількості днів у місяці
 * @param month - номер місяця (1-12)
 * @param year - рік
 * @return int - кількість днів
 */
int getMonthDays(int month, int year)
{
  if (month == 2)
  {
    // Перевірка на високосний рік
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
    {
      return 29;
    }
    else
    {
      return 28;
    }
  }
  else if (month == 4 || month == 6 || month == 9 || month == 11)
  {
    return 30;
  }
  else
  {
    return 31;
  }
}

/**
 * isEventInMonth() - Перевірка чи подія відноситься до вказаного місяця
 * @param timestamp - Unix timestamp події
 * @param month - номер місяця (1-12)
 * @param year - рік
 * @return bool - true якщо подія в цьому місяці
 */
bool isEventInMonth(time_t timestamp, int month, int year)
{
  struct tm *tm_info = localtime(&timestamp);
  return (tm_info->tm_mon + 1 == month && tm_info->tm_year + 1900 == year);
}

/**
 * getMonthsWithEvents() - Отримання списку місяців, в яких є події
 * @return String - список місяців через кому
 */
String getMonthsWithEvents()
{
  String result = "";
  int currentYear = getCurrentYear();
  bool months[13] = {false}; // Масив для відстеження місяців з подіями

  // Перевіряємо всі події
  for (int i = 0; i < eventCount; i++)
  {
    struct tm *tm_info = localtime(&eventLog[i].timestamp);
    int month = tm_info->tm_mon + 1;
    int year = tm_info->tm_year + 1900;
    if (year == currentYear)
    {
      months[month] = true;
    }
  }

  // Формуємо список
  bool first = true;
  for (int i = 1; i <= 12; i++)
  {
    if (months[i])
    {
      if (!first)
        result += ", ";
      result += String(i) + " - " + getMonthName(i);
      first = false;
    }
  }

  if (result == "")
  {
    result = "Немає подій";
  }

  return result;
}

/**
 * getMonthsWithEventsArray() - Отримання масиву місяців з подіями
 * @param monthsArray - масив для заповнення
 * @return int - кількість місяців з подіями
 */
int getMonthsWithEventsArray(int monthsArray[])
{
  int count = 0;
  int currentYear = getCurrentYear();
  bool months[13] = {false};

  for (int i = 0; i < eventCount; i++)
  {
    struct tm *tm_info = localtime(&eventLog[i].timestamp);
    int month = tm_info->tm_mon + 1;
    int year = tm_info->tm_year + 1900;
    if (year == currentYear && !months[month])
    {
      months[month] = true;
    }
  }

  for (int i = 1; i <= 12; i++)
  {
    if (months[i])
    {
      monthsArray[count++] = i;
    }
  }

  return count;
}

// ============================================================
// БЛОК 4: ФУНКЦІЇ РОБОТИ З ФАЙЛАМИ (LittleFS)
// ============================================================

/**
 * saveData() - Збереження даних у JSON файл
 * Зберігає: workTime, onPcTime, offPcTime, eventLog
 */
void saveData()
{
  StaticJsonDocument<4096> doc; // Збільшено для зберігання історії

  // Заповнюємо JSON даними
  doc["workTime"] = workTime;
  doc["onPcTime"] = onPcTime;
  doc["offPcTime"] = offPcTime;
  doc["eventCount"] = eventCount;

  // Зберігаємо історію подій
  JsonArray events = doc.createNestedArray("events");
  for (int i = 0; i < eventCount; i++)
  {
    JsonObject event = events.createNestedObject();
    event["timestamp"] = (unsigned long)eventLog[i].timestamp;
    event["state"] = eventLog[i].state;
    event["voltage"] = eventLog[i].voltage;
  }

  File file = LittleFS.open("/data.json", "w");
  if (!file)
  {
    Serial.println("❌ Помилка відкриття файлу для запису");
    return;
  }

  serializeJson(doc, file);
  file.close();
}

/**
 * loadData() - Завантаження даних з JSON файлу
 * Завантажує: workTime, onPcTime, offPcTime, eventLog
 */
void loadData()
{
  File file = LittleFS.open("/data.json", "r");
  if (!file)
  {
    Serial.println("⚠️ Файл не знайдено, створюємо новий");
    saveData();
    return;
  }

  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    Serial.println("❌ Помилка розбору JSON");
    return;
  }

  // Витягуємо дані з JSON
  workTime = doc["workTime"] | 0;
  onPcTime = doc["onPcTime"] | 0;
  offPcTime = doc["offPcTime"] | 0;
  eventCount = doc["eventCount"] | 0;

  // Завантажуємо історію подій
  JsonArray events = doc["events"];
  int count = min((int)events.size(), MAX_EVENTS);
  for (int i = 0; i < count; i++)
  {
    JsonObject event = events[i];
    eventLog[i].timestamp = (time_t)(event["timestamp"] | 0);
    eventLog[i].state = event["state"] | false;
    eventLog[i].voltage = event["voltage"] | 0.0;
  }
  eventCount = count;

  Serial.println("✅ Дані завантажено! Подій: " + String(eventCount));
  file.close();
}

// ============================================================
// БЛОК 5: ПІДКЛЮЧЕННЯ ДО WI-FI ТА СИНХРОНІЗАЦІЯ ЧАСУ
// ============================================================

/**
 * connectWiFi() - Підключення до Wi-Fi зі списку мереж
 * Перебирає всі мережі з wifiList до першого успішного підключення
 */
void connectWiFi()
{
  Serial.println("\n🔍 Пошук Wi-Fi\n");

  for (int i = 0; i < 3; i++)
  {
    Serial.print("Пробую підключитися до " + String(wifiList[i][0]));

    WiFi.begin(wifiList[i][0], wifiList[i][1]);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < 7000)
    {
      delay(200);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\n✅ Підключено до Wi-Fi: " + String(WiFi.SSID()));
      Serial.println("📡 IP-адреса: " + WiFi.localIP().toString() + "\n");
      return;
    }
  }

  Serial.println("\n❌ Не вдалося підключитися до жодної мережі!");
  delay(1000);
  ESP.restart();
}

/**
 * syncTime() - Синхронізація часу з NTP сервером
 * @return bool - true якщо синхронізація успішна
 */
bool syncTime()
{
  Serial.println("🕐 Синхронізація часу з NTP сервером...");

  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER);

  int attempts = 0;
  while (attempts < 10)
  {
    if (time(nullptr) > 100000)
    {
      Serial.println("✅ Час синхронізовано!");
      time_t now = time(nullptr);
      struct tm *tm_info = localtime(&now);
      Serial.println("📅 Поточний час: " + formatDateTime(now));
      return true;
    }
    delay(1000);
    attempts++;
    Serial.print(".");
  }

  Serial.println("❌ Помилка синхронізації часу!");
  return false;
}

// ============================================================
// БЛОК 6: ОБРОБНИК ПОВІДОМЛЕНЬ TELEGRAM
// ============================================================

/**
 * showMainMenu() - Показує головне меню
 * @param chatID - ID чату для відправки
 */
void showMainMenu(String chatID)
{
  bot.showMenu("🟢 Увімкнути \t 🔴 Вимкнути \n ⏱ Аптайм \t 📊 Стан \t 🔄 Ресет", chatID);
}

/**
 * showMonthMenu() - Показує меню вибору місяця тільки з тими місяцями, де є події
 * @param chatID - ID чату для відправки
 */
void showMonthMenu(String chatID)
{
  int monthsArray[12];
  int monthCount = getMonthsWithEventsArray(monthsArray);

  if (monthCount == 0)
  {
    bot.sendMessage("📭 Немає подій для відображення", chatID);
    return;
  }

  String monthMenu = "";
  for (int i = 0; i < monthCount; i++)
  {
    monthMenu += String(monthsArray[i]) + " - " + getMonthName(monthsArray[i]);
    if (i < monthCount - 1)
      monthMenu += "\n";
  }
  monthMenu += "\n🔙 Назад";
  bot.showMenu(monthMenu, chatID);
}

/*
 * newMsg() - Обробник вхідних повідомлень від Telegram
 * @param msg - структура з даними повідомлення
 */
void newMsg(FB_msg &msg)
{
  float voltage = readVoltage();
  bool pcState = isPcOn();

  Serial.println("\n========================================");
  Serial.print("📩 Отримано: ");
  Serial.println(msg.text);
  Serial.println("📊 Напруга: " + String(voltage, 2) + "V");
  Serial.println("💻 Стан ПК: " + String(pcState ? "УВІМКНЕНО" : "ВИМКНЕНО"));
  Serial.println("========================================\n");

  String msgText = msg.text;
  String from_name = msg.username;
  if (from_name == "")
    from_name = "Аноним";

  // ---- OTA оновлення ----
  if (msg.OTA)
  {
    bot.update();
    bot.sendMessage("Загруженно!", msg.chatID);
    return;
  }

  // ---- Якщо в режимі вибору місяця - обробляємо тільки спеціальні команди ----
  if (inMonthMenu)
  {
    // Команда "Назад" - повертаємося до головного меню
    if (msgText == "🔙 Назад")
    {
      bot.tickManual();
      inMonthMenu = false;
      String response = "🔙 Повернення до головного меню:\n";
      response += "💻 Поточний стан: " + String(pcState ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО");
      bot.sendMessage(response, msg.chatID);
      showMainMenu(msg.chatID);
      return;
    }

    // Команда /start - вихід з режиму вибору місяця
    if (msgText == "/start")
    {
      bot.tickManual();
      inMonthMenu = false;
      bot.sendMessage("👋 Вітаю, " + from_name + "1!1", msg.chatID);
      showMainMenu(msg.chatID);
      return;
    }

    // Команда /remove - видалення даних з перезапуском
    if (msgText == "/remove")
    {
      bot.tickManual();
      inMonthMenu = false;
      if (LittleFS.remove("/data.json"))
      {
        bot.sendMessage("✅ Дані видалено! Перезавантаження...", msg.chatID);
      }
      else
      {
        bot.sendMessage("⚠️ Помилка видалення даних!", msg.chatID);
      }
      delay(1000);
      ESP.restart();
      return;
    }

    // Отримуємо список місяців з подіями
    int monthsArray[12];
    int monthCount = getMonthsWithEventsArray(monthsArray);

    // Перевіряємо чи введене число є в списку доступних місяців
    int selectedMonth = msgText.toInt();
    bool isValidMonth = false;
    for (int i = 0; i < monthCount; i++)
    {
      if (monthsArray[i] == selectedMonth)
      {
        isValidMonth = true;
        break;
      }
    }

    // Обробка вибору місяця (тільки якщо є в списку)
    if (isValidMonth && selectedMonth >= 1 && selectedMonth <= 12)
    {
      bot.tickManual();
      int currentYear = getCurrentYear();

      // Збираємо статистику за вибраний місяць
      int eventsInMonth = 0;
      int onEvents = 0;
      int offEvents = 0;
      time_t firstEventTime = 0;
      time_t lastEventTime = 0;

      // Рахуємо події за місяць
      for (int i = 0; i < eventCount; i++)
      {
        if (isEventInMonth(eventLog[i].timestamp, selectedMonth, currentYear))
        {
          eventsInMonth++;
          if (eventLog[i].state)
          {
            onEvents++;
          }
          else
          {
            offEvents++;
          }
          if (firstEventTime == 0 || eventLog[i].timestamp < firstEventTime)
          {
            firstEventTime = eventLog[i].timestamp;
          }
          if (eventLog[i].timestamp > lastEventTime)
          {
            lastEventTime = eventLog[i].timestamp;
          }
        }
      }

      String response = "📆 **Статистика за " + getMonthName(selectedMonth) + " " + String(currentYear) + "**\n\n";
      response += "📊 Кількість подій: " + String(eventsInMonth) + "\n";
      response += "🟢 Ввімкнень: " + String(onEvents) + "\n";
      response += "🔴 Вимкнень: " + String(offEvents) + "\n";

      if (firstEventTime > 0 && lastEventTime > 0)
      {
        response += "📅 Перша подія: " + formatDateTimeShort(firstEventTime) + "\n";
        response += "📅 Остання подія: " + formatDateTimeShort(lastEventTime) + "\n";
      }
      response += "\n";

      // Показуємо останні 15 подій за місяць
      response += "📋 **Останні події:**\n";
      int displayCount = min(15, eventsInMonth);
      int count = 0;
      for (int i = eventCount - 1; i >= 0 && count < displayCount; i--)
      {
        if (isEventInMonth(eventLog[i].timestamp, selectedMonth, currentYear))
        {
          response += "🕐 " + formatDateTime(eventLog[i].timestamp) + " → ";
          response += String(eventLog[i].state ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО");
          response += " (⚡" + String(eventLog[i].voltage, 2) + "V)\n";
          count++;
        }
      }

      if (eventsInMonth == 0)
      {
        response += "📭 Немає подій за цей місяць\n";
      }

      // Додаємо кнопку для повернення до вибору місяця
      response += "\n🔙 Натисніть **Назад** для повернення до вибору місяця";

      bot.sendMessage(response, msg.chatID);
      return;
    }

    // Якщо в режимі вибору місяця і введено щось інше
    bot.tickManual();
    bot.sendMessage("❌ Будь ласка, виберіть номер місяця зі списку або натисніть Назад", msg.chatID);
    showMonthMenu(msg.chatID);
    return;
  }

  // ---- Якщо НЕ в режимі вибору місяця - обробляємо всі команди ----

  // ---- Команда /start - привітання та меню ----
  if (msgText == "/start")
  {
    bot.tickManual();
    bot.sendMessage("👋 Вітаю, " + from_name + "!", msg.chatID);
    showMainMenu(msg.chatID);
    return;
  }

  // ---- Команда "Увімкнути" - подати живлення на ПК ----
  if (msgText == "🟢 Увімкнути")
  {
    bot.tickManual();
    if (!pcState)
    {
      digitalWrite(relePin, LOW);
      delay(500);
      digitalWrite(relePin, HIGH);
      bot.sendMessage("🔌 Сигнал ввімкнення надіслано!", msg.chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже увімкнено!", msg.chatID);
    }
    return;
  }

  // ---- Команда "Вимкнути" - вимкнути живлення ПК ----
  if (msgText == "🔴 Вимкнути")
  {
    bot.tickManual();
    if (pcState)
    {
      digitalWrite(relePin, LOW);
      delay(500);
      digitalWrite(relePin, HIGH);
      bot.sendMessage("🔌 Сигнал вимкнення надіслано!", msg.chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже вимкнено!", msg.chatID);
    }
    return;
  }

  // ---- Команда "Стан" - поточна інформація ----
  if (msgText == "📊 Стан")
  {
    bot.tickManual();
    String status = pcState ? "✅ працює" : "❌ не працює";
    String response = myNameBot + " " + status + "\n";
    response += "📊 Напруга: " + String(voltage, 2) + "V\n";
    response += "📡 Wi-Fi: " + String(WiFi.SSID()) + "\n";
    response += "📶 Сигнал: " + String(WiFi.RSSI()) + " dBm\n";
    response += "🕐 Поточний час: " + formatDateTime(time(nullptr)) + "\n";
    response += "🆔 Chat ID: " + String(msg.chatID);
    bot.sendMessage(response, msg.chatID);
    return;
  }

  // ---- Команда "Аптайм" - показуємо меню вибору місяця ----
  if (msgText == "⏱ Аптайм")
  {
    bot.tickManual();
    inMonthMenu = true;

    int currentMonth = getCurrentMonth();
    int currentYear = getCurrentYear();

    // Отримуємо список місяців з подіями
    int monthsArray[12];
    int monthCount = getMonthsWithEventsArray(monthsArray);

    String monthList = getMonthsWithEvents();

    // Основна інформація
    String response = "📅 **" + myNameBot + "**\n";
    response += "🕐 Поточний час: " + formatDateTime(time(nullptr)) + "\n\n";
    response += "📌 Загальний час: " + formatTime(workTime) + "\n\n";
    response += "🟢 ПК працює: " + formatTime(onPcTime) + "\n";
    response += "🔴 ПК вимкнено: " + formatTime(offPcTime) + "\n\n";
    response += "📊 Кількість подій: " + String(eventCount) + "\n";
    response += "📆 Поточний місяць: " + getMonthName(currentMonth) + " " + String(currentYear) + "\n\n";

    if (monthCount > 0)
    {
      response += "📋 **Доступні місяці з подіями:**\n";
      response += monthList + "\n\n";
      response += "🗓 **Виберіть номер місяця зі списку або натисніть Назад:**";
    }
    else
    {
      response += "📭 Немає подій для відображення";
    }

    bot.sendMessage(response, msg.chatID);

    if (monthCount > 0)
    {
      showMonthMenu(msg.chatID);
    }

    return;
  }

  // ---- Команда "Ресет" - перезавантаження WEMOS ----
  if (msgText == "🔄 Ресет")
  {
    bot.tickManual();
    bot.sendMessage(myNameBot + ": Перезавантажується.....", msg.chatID);
    delay(500);
    ESP.restart();
    return;
  }

  // ---- Команда "/remove" - видалення даних з перезапуском ----
  if (msgText == "/remove")
  {
    bot.tickManual();
    if (LittleFS.remove("/data.json"))
    {
      bot.sendMessage("✅ Дані видалено! Перезавантаження...", msg.chatID);
    }
    else
    {
      bot.sendMessage("⚠️ Помилка видалення даних!", msg.chatID);
    }
    delay(1000);
    ESP.restart();
    return;
  }

  // ---- Невідома команда ----
  if (!msgText.endsWith(".bin"))
  {
    bot.tickManual();
    bot.sendMessage("📩 Невідома команда: \"" + msgText + "\"\nНапишіть /start", msg.chatID);
  }
}

// ============================================================
// БЛОК 7: SETUP() - ІНІЦІАЛІЗАЦІЯ
// ============================================================

void setup()
{
  // ---- Ініціалізація Serial ----
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n🚀 Запуск WEMOS...");
  delay(3000);

  // ---- Ініціалізація LittleFS ----
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

  // ---- Налаштування пінів ----
  pinMode(POWER_PIN, INPUT);
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, HIGH);

  // ---- Завантаження збережених даних ----
  loadData();

  // ---- Підключення до Wi-Fi ----
  rowsWifiList = (sizeof(wifiList) / sizeof(wifiList[0]));
  connectWiFi();

  // ---- Синхронізація часу ----
  syncTime();

  // ---- Визначення початкового стану ПК ----
  float voltage = readVoltage();
  pcIsOn = (voltage > VOLTAGE_ON);
  prevPcState = pcIsOn;

  // ---- Додаємо першу подію в історію ----
  if (eventCount < MAX_EVENTS)
  {
    eventLog[eventCount].timestamp = time(nullptr);
    eventLog[eventCount].state = pcIsOn;
    eventLog[eventCount].voltage = voltage;
    eventCount++;
  }

  Serial.println("📊 Початкова напруга: " + String(voltage, 2) + "V");
  Serial.println("💻 Стан ПК: " + String(pcIsOn ? "УВІМКНЕНО" : "ВИМКНЕНО"));
  Serial.println("🕐 Поточний час: " + formatDateTime(time(nullptr)));

  // ---- Ініціалізація Telegram бота ----
  bot.attach(newMsg);
  delay(1000);

  // ---- Відправка привітального повідомлення ----
  Serial.println("\n✅ WEMOS: увімкнувся!");
  String startMsg = "✅ WEMOS: увімкнувся!\n";
  startMsg += "💻 ПК: " + String(pcIsOn ? "✅ працює" : "❌ не працює");
  startMsg += "\n📊 Напруга: " + String(voltage, 2) + "V";
  startMsg += "\n🕐 " + formatDateTime(time(nullptr));
  bot.sendMessage(startMsg, CHAT_ID_ADMIN);

  // ---- Ініціалізація таймерів ----
  lastMillis = millis();
  lastSaveTime = millis();
  lastNtpUpdate = millis();
  inMonthMenu = false;
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
    connectWiFi();
  }

  bot.tick();

  // ---- Оновлення часу з NTP (кожну годину) ----
  unsigned long currentMillis = millis();
  if (currentMillis - lastNtpUpdate > NTP_UPDATE_INTERVAL)
  {
    lastNtpUpdate = currentMillis;
    syncTime();
  }

  // ---- Оновлення таймерів часу ----
  unsigned long delta = currentMillis - lastMillis;

  if (delta > 0)
  {
    workTime += delta;

    if (pcIsOn)
    {
      onPcTime += delta;
    }
    else
    {
      offPcTime += delta;
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
    Serial.print(" | PC: " + String(newPcState ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО"));
    Serial.println(" | 🕐 " + formatDateTime(time(nullptr)));
  }

  // ---- Відправка сповіщень при зміні стану ПК та запис в історію ----
  if (newPcState != pcIsOn)
  {
    pcIsOn = newPcState;

    // Додаємо подію в історію
    if (eventCount < MAX_EVENTS)
    {
      eventLog[eventCount].timestamp = time(nullptr);
      eventLog[eventCount].state = pcIsOn;
      eventLog[eventCount].voltage = voltage;
      eventCount++;

      // Якщо досягнуто ліміту, видаляємо найстаріші події
      if (eventCount >= MAX_EVENTS)
      {
        // Зсуваємо всі події на одну позицію назад
        for (int i = 0; i < MAX_EVENTS - 1; i++)
        {
          eventLog[i] = eventLog[i + 1];
        }
        eventCount = MAX_EVENTS - 1;
      }
    }

    if (pcIsOn)
    {
      Serial.println("💻 ПК УВІМКНУВСЯ!");
      bot.sendMessage("✅ ПК ввімкнувся!\n📊 Напруга: " + String(voltage, 2) + "V\n🕐 " + formatDateTime(time(nullptr)), CHAT_ID_ADMIN);
    }
    else
    {
      Serial.println("💻 ПК ВИМКНУВСЯ!");
      bot.sendMessage("❌ ПК вимкнувся!\n📊 Напруга: " + String(voltage, 2) + "V\n🕐 " + formatDateTime(time(nullptr)), CHAT_ID_ADMIN);
    }
  }

  // ---- Автоматичне збереження даних (кожну секунду) ----
  if (currentMillis - lastSaveTime > 1000)
  {
    lastSaveTime = currentMillis;
    saveData();
  }

  // ---- Планове перезавантаження кожні 24 години ----
  static unsigned long lastRestart = 0;
  if (currentMillis - lastRestart >= 86400000UL)
  {
    lastRestart = currentMillis;
    Serial.println("🔄 Перезавантаження через 24 години...");
    saveData();
    bot.sendMessage("🔄 Планове перезавантаження...\n🕐 " + formatDateTime(time(nullptr)), CHAT_ID_ADMIN);
    delay(1000);
    ESP.restart();
  }

  delay(10);
}