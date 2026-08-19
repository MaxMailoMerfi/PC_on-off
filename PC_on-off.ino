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
// БЛОК 1: ПІДКЛЮЧЕННЯ БІБЛІОТЕК
// ============================================================

#include <FastBot.h>     // Бібліотека для роботи з Telegram Bot API (відправка/прийом повідомлень)
#include <LittleFS.h>    // Вбудована файлова система ESP8266 для зберігання даних у Flash-пам'яті
#include <ArduinoJson.h> // Потужна бібліотека для парсингу та створення JSON (зберігання структурованих даних)
#include <time.h>        // Стандартна бібліотека для роботи з часом (Unix timestamp, struct tm)

// ============================================================
// БЛОК 2: КОНФІГУРАЦІЯ ПРОЄКТУ (ВСІ НАЛАШТУВАННЯ В ОДНОМУ МІСЦІ)
// ============================================================

// ---- 2.1 Налаштування Telegram Bot ----
#define BOT_TOKEN "8487247789:AAHBN9wKzSVsCCKi4cJYdYb2Nh83e_itIYE" // Токен бота від @BotFather (унікальний ключ доступу)
#define CHAT_ID_ADMIN "1031379571"                                 // ID адміністратора (кому будуть приходити сповіщення про запуск та зміни стану)

// ---- 2.2 Налаштування апаратних пінів (GPIO) ----
#define relePin 4    // Пін керування реле (GPIO4 = D2 на WEMOS D1) - сигнал для ввімкнення/вимкнення ПК
#define POWER_PIN A0 // Пін вимірювання напруги (A0 - аналоговий вхід) - підключається до дільника напруги з блоку живлення ПК

// ---- 2.3 Налаштування порогів вимірювання напруги ----
#define VOLTAGE_REF 5.0 // Опорна напруга (залежить від дільника напруги на вході A0)
#define VOLTAGE_ON 3.0  // Поріг ввімкнення ПК (>3V означає, що ПК працює і споживає струм)
#define VOLTAGE_OFF 2.7 // Поріг вимкнення ПК (<2.7V означає, що ПК вимкнено або в режимі сну)
                        // Гістерезис 0.3V між порогами для стабільності

// ---- 2.4 Налаштування NTP (синхронізація точного часу через інтернет) ----
#define NTP_SERVER "pool.ntp.org"   // Адреса NTP сервера (пул серверів точного часу)
#define TIMEZONE_OFFSET 3           // Часовий пояс (UTC+3 для Києва/Київського часу)
#define NTP_UPDATE_INTERVAL 3600000 // Оновлення часу кожну годину (3600000 мілісекунд = 1 година)

// ---- 2.5 Список Wi-Fi мереж для автоматичного підключення (резервні мережі) ----
const char *wifiList[][2] = {
    {"deti_podzemelia", "12345678"}, // Перша мережа: назва, пароль
    {"NewZAG", "12345678"},          // Друга мережа (резервна)
    {"Xiaomi 14T", "12345678"}       // Третя мережа (резервна)
};

// ============================================================
// БЛОК 3: СТВОРЕННЯ ОБ'ЄКТІВ ТА ГЛОБАЛЬНІ ЗМІННІ
// ============================================================

FastBot bot(BOT_TOKEN); // Створюємо екземпляр бота з нашим токеном

// ---- 3.1 Загальні змінні стану ----
String myNameBot = "Головний комп'ютер"; // Ім'я пристрою (відображається в повідомленнях)
bool pcIsOn = false;                     // Поточний логічний стан ПК: true = увімкнено, false = вимкнено
bool prevPcState = false;                // Попередній стан (використовується для відправки сповіщень ТІЛЬКИ при зміні)
int rowsWifiList;                        // Кількість Wi-Fi мереж у списку (обчислюється автоматично)
bool inMonthMenu = false;                // Прапорець: чи знаходимося в режимі вибору місяця (блокує інші команди)

// ---- 3.2 Змінні для відліку часу роботи ----
unsigned long workTime = 0;      // Загальний час роботи WEMOS (в мілісекундах) - рахується від запуску
unsigned long onPcTime = 0;      // Сумарний час, коли ПК був увімкнений (в мілісекундах)
unsigned long offPcTime = 0;     // Сумарний час, коли ПК був вимкнений (в мілісекундах)
unsigned long lastMillis = 0;    // Час останнього оновлення лічильників (для обчислення дельти)
unsigned long lastSaveTime = 0;  // Час останнього збереження даних у файл (щоб не зберігати занадто часто)
unsigned long lastNtpUpdate = 0; // Час останнього оновлення NTP (щоб не запитувати час занадто часто)

// ---- 3.3 Структура для історії подій (лог ввімкнень/вимкнень ПК) ----
struct EventLog
{
  time_t timestamp; // Час події у форматі Unix timestamp (кількість секунд з 01.01.1970)
  bool state;       // Стан ПК в момент події: true = ввімкнено, false = вимкнено
  float voltage;    // Напруга на дільнику в момент події (для діагностики)
};

#define MAX_EVENTS 1000        // Максимальна кількість подій, які зберігаються в пам'яті
EventLog eventLog[MAX_EVENTS]; // Масив для зберігання історії подій (працює як кільцевий буфер)
int eventCount = 0;            // Поточна кількість подій у масиві

// ---- 3.4 Змінні для роботи з часом ----
time_t currentTime = 0; // Поточний Unix timestamp (оновлюється кожну годину через NTP)
struct tm timeinfo;     // Структура для збереження розібраного часу

// ============================================================
// БЛОК 4: ГОЛОВНИЙ ЦИКЛ (LOOP) - ВИЩИЙ РІВЕНЬ, ТУТ ВИДНО ЩО ВІДБУВАЄТЬСЯ
// ============================================================

// ------------------------------------------------------------
// 4.1 loop - ГОЛОВНИЙ ЦИКЛ ПРОГРАМИ (виконується нескінченно)
// ------------------------------------------------------------
// Тут видно ЗАГАЛЬНУ КАРТИНУ: що робить програма кожну мілісекунду
// Деталі КОЖНОЇ дії описані в функціях нижче
// ------------------------------------------------------------
void loop()
{
  unsigned long currentMillis = millis(); // Отримуємо поточний час у мілісекундах

  // ---- Крок 1: Слідкуємо за Wi-Fi (якщо відключився - перепідключаємо) ----
  checkWiFiConnection();

  // ---- Крок 2: Обробляємо вхідні повідомлення від Telegram ----
  bot.tick(); // Це важливо! Без цього бот не буде отримувати повідомлення

  // ---- Крок 3: Оновлюємо точний час з інтернету (кожну годину) ----
  updateNtpTime(currentMillis);

  // ---- Крок 4: Оновлюємо лічильники часу (скільки працюємо, скільки ПК ввімкнено) ----
  updateTimeCounters(currentMillis);

  // ---- Крок 5: Вимірюємо напругу та визначаємо стан ПК ----
  checkPcState(currentMillis);

  // ---- Крок 6: Автоматично зберігаємо дані у файл (кожну секунду) ----
  autoSaveData(currentMillis);

  // ---- Крок 7: Перевіряємо, чи не пора перезавантажитись (кожні 24 години) ----
  checkScheduledRestart(currentMillis);

  delay(10); // Невелика затримка для зменшення навантаження на процесор
}

// ============================================================
// БЛОК 5: ФУНКЦІЇ, ЯКІ ВИКОРИСТОВУЮТЬСЯ В LOOP (СЕРЕДНІЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 5.1 checkWiFiConnection - Перевірка та відновлення Wi-Fi
// ------------------------------------------------------------
// Що робить: перевіряє чи є підключення до Wi-Fi, якщо ні - перепідключає
// Викликається з: loop() (крок 1)
// ------------------------------------------------------------
void checkWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠️ Wi-Fi відключено! Перепідключаюсь...");
    connectWiFi(); // Викликаємо функцію підключення (описана нижче)
  }
}

// ------------------------------------------------------------
// 5.2 updateNtpTime - Оновлення часу з NTP сервером
// ------------------------------------------------------------
// Що робить: оновлює системний час через NTP кожну годину
// Викликається з: loop() (крок 3)
// ------------------------------------------------------------
void updateNtpTime(unsigned long currentMillis)
{
  if (currentMillis - lastNtpUpdate > NTP_UPDATE_INTERVAL)
  {
    lastNtpUpdate = currentMillis;
    syncTime(); // Викликаємо функцію синхронізації (описана нижче)
  }
}

// ------------------------------------------------------------
// 5.3 updateTimeCounters - Оновлення лічильників часу
// ------------------------------------------------------------
// Що робить: оновлює workTime, onPcTime, offPcTime на основі дельти часу
// Викликається з: loop() (крок 4)
// ------------------------------------------------------------
void updateTimeCounters(unsigned long currentMillis)
{
  unsigned long delta = currentMillis - lastMillis; // Скільки часу минуло з останнього оновлення

  if (delta > 0)
  {
    workTime += delta; // Додаємо до загального часу роботи WEMOS

    if (pcIsOn)
    {
      onPcTime += delta; // Якщо ПК увімкнено - додаємо до onPcTime
    }
    else
    {
      offPcTime += delta; // Якщо ПК вимкнено - додаємо до offPcTime
    }
  }
  lastMillis = currentMillis; // Запам'ятовуємо поточний час
}

// ------------------------------------------------------------
// 5.4 checkPcState - Перевірка стану ПК та обробка змін
// ------------------------------------------------------------
// Що робить: вимірює напругу, визначає стан ПК, логує та обробляє зміни
// Викликається з: loop() (крок 5)
// ------------------------------------------------------------
void checkPcState(unsigned long currentMillis)
{
  float voltage = readVoltage(); // Вимірюємо напругу (функція описана нижче)
  bool newPcState = isPcOn();    // Визначаємо стан ПК (функція описана нижче)

  // ---- Логування стану (кожні 10 секунд або при зміні) ----
  static unsigned long lastPrint = 0;
  if (newPcState != pcIsOn || currentMillis - lastPrint > 10000)
  {
    lastPrint = currentMillis;
    Serial.print("📊 Напруга: " + String(voltage, 2) + "V");
    Serial.print(" | PC: " + String(newPcState ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО"));
    Serial.println(" | 🕐 " + formatDateTime(time(nullptr))); // formatDateTime описана нижче
  }

  // ---- Якщо стан ПК змінився - обробляємо зміну ----
  if (newPcState != pcIsOn)
  {
    pcIsOn = newPcState;                  // Оновлюємо глобальний стан
    addEventToLog(voltage);               // Додаємо подію в історію (функція нижче)
    sendStateChangeNotification(voltage); // Надсилаємо сповіщення (функція нижче)
  }
}

// ------------------------------------------------------------
// 5.5 autoSaveData - Автоматичне збереження даних у файл
// ------------------------------------------------------------
// Що робить: зберігає всі дані у файл кожну секунду
// Викликається з: loop() (крок 6)
// ------------------------------------------------------------
void autoSaveData(unsigned long currentMillis)
{
  if (currentMillis - lastSaveTime > 1000) // Якщо минула 1 секунда
  {
    lastSaveTime = currentMillis;
    saveData(); // Викликаємо функцію збереження (описана нижче)
  }
}

// ------------------------------------------------------------
// 5.6 checkScheduledRestart - Перевірка планового перезавантаження
// ------------------------------------------------------------
// Що робить: автоматично перезавантажує WEMOS кожні 24 години
// Викликається з: loop() (крок 7)
// ------------------------------------------------------------
void checkScheduledRestart(unsigned long currentMillis)
{
  static unsigned long lastRestart = 0;          // Статична змінна - зберігає значення між викликами
  if (currentMillis - lastRestart >= 86400000UL) // 86400000 мс = 24 години
  {
    lastRestart = currentMillis;
    Serial.println("🔄 Перезавантаження через 24 години...");
    saveData(); // Зберігаємо дані перед перезавантаженням

    bot.sendMessage("🔄 Планове перезавантаження...\n🕐 " + formatDateTime(time(nullptr)), CHAT_ID_ADMIN);
    delay(1000);
    ESP.restart(); // Перезавантажуємо модуль
  }
}

// ============================================================
// БЛОК 6: ОБРОБНИК ТЕЛЕГРАМ ПОВІДОМЛЕНЬ (ВИЩИЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 6.1 newMsg - ОСНОВНИЙ ОБРОБНИК ВХІДНИХ ПОВІДОМЛЕНЬ ВІД TELEGRAM
// ------------------------------------------------------------
// Що робить: приймає повідомлення від користувача і вирішує, що з ним робити
// Це головна функція, яку викликає бібліотека FastBot
// ------------------------------------------------------------
void newMsg(FB_msg &msg)
{
  float voltage = readVoltage();
  bool pcState = isPcOn();

  // ---- Логування отриманого повідомлення (для налагодження) ----
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

  // ---- OTA (Over-The-Air) оновлення прошивки ----
  if (msg.OTA)
  {
    bot.update(); // Запускаємо процес оновлення
  }

  // ---- Режим вибору місяця (блоковані всі команди, крім вибору та "Назад") ----
  if (inMonthMenu)
  {
    processMonthSelection(msgText, msg.chatID); // Функція описана нижче
    return;
  }

  // ---- Основний режим (обробляємо всі стандартні команди) ----
  processMainCommands(msgText, msg.chatID, from_name); // Функція описана нижче
}

// ------------------------------------------------------------
// 6.2 processMainCommands - Обробка основних команд бота
// ------------------------------------------------------------
// Що робить: обробляє всі команди, крім вибору місяця
// Команди: /start, 🟢 Увімкнути, 🔴 Вимкнути, 📊 Стан, ⏱ Аптайм, 🔄 Ресет, /remove
// ------------------------------------------------------------
void processMainCommands(String msgText, String chatID, String from_name)
{
  // ---- /start - привітання та показ головного меню ----
  if (msgText == "/start")
  {
    bot.tickManual();
    bot.sendMessage("👋 Вітаю, " + from_name + "!", chatID);
    showMainMenu(chatID); // Функція описана нижче
    return;
  }

  // ---- 🟢 Увімкнути - подаємо сигнал на ввімкнення ПК ----
  if (msgText == "🟢 Увімкнути")
  {
    bot.tickManual();
    if (!isPcOn()) // Якщо ПК вимкнено
    {
      digitalWrite(relePin, LOW);  // Замикаємо реле (імітація натискання кнопки)
      delay(500);                  // Тримаємо 500 мс
      digitalWrite(relePin, HIGH); // Розмикаємо реле
      bot.sendMessage("🔌 Сигнал ввімкнення надіслано!", chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже увімкнено!", chatID);
    }
    return;
  }

  // ---- 🔴 Вимкнути - подаємо сигнал на вимкнення ПК ----
  if (msgText == "🔴 Вимкнути")
  {
    bot.tickManual();
    if (isPcOn()) // Якщо ПК увімкнено
    {
      digitalWrite(relePin, LOW);  // Замикаємо реле
      delay(500);                  // Тримаємо 500 мс
      digitalWrite(relePin, HIGH); // Розмикаємо реле
      bot.sendMessage("🔌 Сигнал вимкнення надіслано!", chatID);
    }
    else
    {
      bot.sendMessage("✅ ПК вже вимкнено!", chatID);
    }
    return;
  }

  // ---- 📊 Стан - показуємо поточний статус ----
  if (msgText == "📊 Стан")
  {
    bot.tickManual();
    sendStatusMessage(chatID); // Функція описана нижче
    return;
  }

  // ---- ⏱ Аптайм - показуємо меню вибору місяця зі статистикою ----
  if (msgText == "⏱ Аптайм")
  {
    bot.tickManual();
    showUptimeMenu(chatID); // Функція описана нижче
    return;
  }

  // ---- 🔄 Ресет - перезавантаження WEMOS ----
  if (msgText == "🔄 Ресет")
  {
    bot.tickManual();
    bot.sendMessage(myNameBot + ": Перезавантажується.....", chatID);
    delay(500);
    ESP.restart(); // Перезавантажуємо модуль
    return;
  }

  // ---- /remove - видалення всіх даних з перезавантаженням ----
  if (msgText == "/remove")
  {
    bot.tickManual();
    if (LittleFS.remove("/data.json")) // Видаляємо файл з даними
    {
      bot.sendMessage("✅ Дані видалено! Перезавантаження...", chatID);
    }
    else
    {
      bot.sendMessage("⚠️ Помилка видалення даних!", chatID);
    }
    delay(1000);
    ESP.restart(); // Перезавантажуємо модуль
    return;
  }

  // ---- Невідома команда ----
  if (!msgText.endsWith(".bin")) // Ігноруємо файли з оновленнями
  {
    bot.tickManual();
    bot.sendMessage("📩 Невідома команда: \"" + msgText + "\"\nНапишіть /start", chatID);
  }
}

// ------------------------------------------------------------
// 6.3 processMonthSelection - Обробка вибору місяця
// ------------------------------------------------------------
// Що робить: обробляє натискання на кнопки місяців або "Назад"
// Алгоритм: якщо "Назад" - виходимо з режиму, якщо номер місяця - показуємо статистику
// ------------------------------------------------------------
void processMonthSelection(String msgText, String chatID)
{
  // ---- Команда "Назад" - повертаємося в головне меню ----
  if (msgText == "🔙 Назад")
  {
    bot.tickManual();
    inMonthMenu = false; // Виходимо з режиму вибору місяця

    String response = "🔙 Повернення до головного меню:\n";
    response += "💻 Поточний стан: " + String(isPcOn() ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО");
    bot.sendMessage(response, chatID);
    showMainMenu(chatID); // Показуємо головне меню
    return;
  }

  // ---- Отримуємо список доступних місяців ----
  int monthsArray[12];
  int monthCount = getMonthsWithEventsArray(monthsArray); // Функція описана нижче

  // ---- Перевіряємо, чи введене число є в списку ----
  int selectedMonth = msgText.toInt(); // Перетворюємо текст у число
  bool isValidMonth = false;
  for (int i = 0; i < monthCount; i++)
  {
    if (monthsArray[i] == selectedMonth)
    {
      isValidMonth = true;
      break;
    }
  }

  // ---- Якщо вибрано коректний місяць - показуємо статистику ----
  if (isValidMonth && selectedMonth >= 1 && selectedMonth <= 12)
  {
    bot.tickManual();
    displayMonthStatistics(selectedMonth, chatID); // Функція описана нижче
    return;
  }

  // ---- Якщо введено щось некоректне -------
  bot.tickManual();
  bot.sendMessage("❌ Будь ласка, виберіть номер місяця зі списку або натисніть Назад", chatID);
  showMonthMenu(chatID); // Показуємо меню знову
}

// ============================================================
// БЛОК 7: ТЕЛЕГРАМ МЕНЮ ТА ІНТЕРФЕЙС (СЕРЕДНІЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 7.1 showMainMenu - Показує головне меню з кнопками
// ------------------------------------------------------------
// Що робить: надсилає повідомлення з кнопками для основних дій
// ------------------------------------------------------------
void showMainMenu(String chatID)
{
  // showMenu() створює повідомлення з кнопками (розділені табуляцією \t та переносом \n)
  bot.showMenu("🟢 Увімкнути \t 🔴 Вимкнути \n ⏱ Аптайм \t 📊 Стан \t 🔄 Ресет", chatID);
}

// ------------------------------------------------------------
// 7.2 sendStatusMessage - Відправка повідомлення з поточним статусом
// ------------------------------------------------------------
// Що робить: надсилає детальну інформацію про стан системи
// Містить: стан ПК, напругу, Wi-Fi, сигнал, час, Chat ID
// ------------------------------------------------------------
void sendStatusMessage(String chatID)
{
  float voltage = readVoltage(); // Вимірюємо напругу (функція описана нижче)
  bool pcState = isPcOn();       // Визначаємо стан ПК (функція описана нижче)
  String status = pcState ? "✅ працює" : "❌ не працює";

  // ---- Формуємо повідомлення ----
  String response = myNameBot + " " + status + "\n";
  response += "📊 Напруга: " + String(voltage, 2) + "V\n";
  response += "📡 Wi-Fi: " + String(WiFi.SSID()) + "\n";
  response += "📶 Сигнал: " + String(WiFi.RSSI()) + " dBm\n";
  response += "🕐 Поточний час: " + formatDateTime(time(nullptr)) + "\n"; // formatDateTime описана нижче
  response += "🆔 Chat ID: " + String(chatID);

  bot.sendMessage(response, chatID); // Надсилаємо
}

// ------------------------------------------------------------
// 7.3 showUptimeMenu - Показ повного меню аптайму
// ------------------------------------------------------------
// Що робить: показує загальну статистику та меню вибору місяця
// Особливість: встановлює прапорець inMonthMenu = true (блокує інші команди)
// ------------------------------------------------------------
void showUptimeMenu(String chatID)
{
  inMonthMenu = true; // Входимо в режим вибору місяця (команди блокуються)

  int currentMonth = getCurrentMonth(); // Отримуємо поточний місяць (функція нижче)
  int currentYear = getCurrentYear();   // Отримуємо поточний рік (функція нижче)

  int monthsArray[12];
  int monthCount = getMonthsWithEventsArray(monthsArray);
  String monthList = getMonthsWithEvents(); // Список місяців з подіями (функція нижче)

  // ---- Формуємо основне повідомлення зі статистикою ----
  String response = "📅 **" + myNameBot + "**\n";
  response += "🕐 Поточний час: " + formatDateTime(time(nullptr)) + "\n\n";
  response += "📌 Загальний час роботи WEMOS: " + formatTime(workTime) + "\n\n"; // formatTime нижче
  response += "🟢 ПК працював: " + formatTime(onPcTime) + "\n";
  response += "🔴 ПК був вимкнений: " + formatTime(offPcTime) + "\n\n";
  response += "📊 Всього подій: " + String(eventCount) + "\n";
  response += "📆 Поточний місяць: " + getMonthName(currentMonth) + " " + String(currentYear) + "\n\n";

  // ---- Додаємо список доступних місяців ----
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

  bot.sendMessage(response, chatID); // Надсилаємо статистику

  // ---- Якщо є події - показуємо меню вибору місяця ----
  if (monthCount > 0)
  {
    showMonthMenu(chatID); // Функція нижче
  }
}

// ------------------------------------------------------------
// 7.4 showMonthMenu - Показує меню вибору місяця
// ------------------------------------------------------------
// Що робить: створює кнопки з номерами місяців, де є події
// Особливість: відображає ТІЛЬКИ ті місяці, де є хоча б одна подія
// ------------------------------------------------------------
void showMonthMenu(String chatID)
{
  int monthsArray[12];
  int monthCount = getMonthsWithEventsArray(monthsArray); // Функція нижче

  // ---- Якщо подій немає взагалі ----
  if (monthCount == 0)
  {
    bot.sendMessage("📭 Немає подій для відображення", chatID);
    return;
  }

  // ---- Формуємо рядок з кнопками для кожного місяця ----
  String monthMenu = "";
  for (int i = 0; i < monthCount; i++)
  {
    monthMenu += String(monthsArray[i]) + " - " + getMonthName(monthsArray[i]); // getMonthName нижче
    if (i < monthCount - 1)
      monthMenu += "\n";
  }
  monthMenu += "\n🔙 Назад"; // Додаємо кнопку повернення

  bot.showMenu(monthMenu, chatID); // Показуємо меню
}

// ------------------------------------------------------------
// 7.5 displayMonthStatistics - Відображення статистики за вибраний місяць
// ------------------------------------------------------------
// Що робить: показує детальну статистику за вибраний місяць
// Містить: кількість подій, ввімкнень/вимкнень, перша/остання подія, список останніх 15 подій
// ------------------------------------------------------------
void displayMonthStatistics(int selectedMonth, String chatID)
{
  int currentYear = getCurrentYear(); // Функція нижче

  // ---- Збираємо статистику за місяць ----
  int eventsInMonth = 0;     // Всього подій у місяці
  int onEvents = 0;          // Кількість ввімкнень
  int offEvents = 0;         // Кількість вимкнень
  time_t firstEventTime = 0; // Час першої події
  time_t lastEventTime = 0;  // Час останньої події

  // Проходимо по всіх подіях
  for (int i = 0; i < eventCount; i++)
  {
    if (isEventInMonth(eventLog[i].timestamp, selectedMonth, currentYear)) // Функція нижче
    {
      eventsInMonth++;
      if (eventLog[i].state)
        onEvents++; // Ввімкнення
      else
        offEvents++; // Вимкнення

      // Оновлюємо першу та останню подію
      if (firstEventTime == 0 || eventLog[i].timestamp < firstEventTime)
        firstEventTime = eventLog[i].timestamp;
      if (eventLog[i].timestamp > lastEventTime)
        lastEventTime = eventLog[i].timestamp;
    }
  }

  // ---- Формуємо повідомлення зі статистикою ----
  String response = "📆 **Статистика за " + getMonthName(selectedMonth) + " " + String(currentYear) + "**\n\n";
  response += "📊 Кількість подій: " + String(eventsInMonth) + "\n";
  response += "🟢 Ввімкнень: " + String(onEvents) + "\n";
  response += "🔴 Вимкнень: " + String(offEvents) + "\n";

  if (firstEventTime > 0 && lastEventTime > 0)
  {
    response += "📅 Перша подія: " + formatDateTimeShort(firstEventTime) + "\n"; // formatDateTimeShort нижче
    response += "📅 Остання подія: " + formatDateTimeShort(lastEventTime) + "\n";
  }
  response += "\n";

  // ---- Показуємо останні 15 подій за місяць ----
  response += "📋 **Останні події:**\n";
  int displayCount = min(15, eventsInMonth);
  int count = 0;

  for (int i = eventCount - 1; i >= 0 && count < displayCount; i--)
  {
    if (isEventInMonth(eventLog[i].timestamp, selectedMonth, currentYear))
    {
      response += "🕐 " + formatDateTime(eventLog[i].timestamp) + " → "; // formatDateTime нижче
      response += String(eventLog[i].state ? "✅ УВІМКНЕНО" : "❌ ВИМКНЕНО");
      response += " (⚡" + String(eventLog[i].voltage, 2) + "V)\n";
      count++;
    }
  }

  if (eventsInMonth == 0)
  {
    response += "📭 Немає подій за цей місяць\n";
  }

  response += "\n🔙 Натисніть **Назад** для повернення до вибору місяця";

  bot.sendMessage(response, chatID);
}

// ============================================================
// БЛОК 8: ФУНКЦІЇ РОБОТИ З МЕРЕЖЕЮ ТА ЧАСОМ (НИЖЧИЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 8.1 connectWiFi - Підключення до Wi-Fi зі списку мереж
// ------------------------------------------------------------
// Що робить: автоматично підключається до першої доступної мережі зі списку
// Алгоритм: перебирає всі мережі, на кожну виділяє 7 секунд
// Якщо жодна не підключилась - перезавантажується
// ------------------------------------------------------------
void connectWiFi()
{
  Serial.println("\n🔍 Пошук Wi-Fi\n");

  for (int i = 0; i < 3; i++) // 3 - кількість мереж у wifiList
  {
    Serial.print("Пробую підключитися до " + String(wifiList[i][0]));

    WiFi.begin(wifiList[i][0], wifiList[i][1]); // Спроба підключення

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
  ESP.restart(); // Перезавантажуємо модуль
}

// ------------------------------------------------------------
// 8.2 syncTime - Синхронізація часу з NTP сервером
// ------------------------------------------------------------
// Що робить: отримує точний час з інтернету через NTP протокол
// Повертає: bool - true якщо синхронізація успішна
// ------------------------------------------------------------
bool syncTime()
{
  Serial.println("🕐 Синхронізація часу з NTP сервером...");

  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER); // Налаштовуємо NTP

  int attempts = 0;
  while (attempts < 10)
  {
    if (time(nullptr) > 100000) // Якщо час отримано
    {
      Serial.println("✅ Час синхронізовано!");
      time_t now = time(nullptr);
      Serial.println("📅 Поточний час: " + formatDateTime(now)); // formatDateTime нижче
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
// БЛОК 9: ФУНКЦІЇ РОБОТИ З ДАТАМИ (НИЖЧИЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 9.1 getCurrentMonth - Отримання поточного місяця
// ------------------------------------------------------------
int getCurrentMonth()
{
  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);
  return tm_info->tm_mon + 1; // tm_mon повертає 0-11, тому додаємо 1
}

// ------------------------------------------------------------
// 9.2 getCurrentYear - Отримання поточного року
// ------------------------------------------------------------
int getCurrentYear()
{
  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);
  return tm_info->tm_year + 1900; // tm_year рахує роки з 1900
}

// ------------------------------------------------------------
// 9.3 isEventInMonth - Перевірка, чи належить подія до вказаного місяця
// ------------------------------------------------------------
bool isEventInMonth(time_t timestamp, int month, int year)
{
  struct tm *tm_info = localtime(&timestamp);
  return (tm_info->tm_mon + 1 == month && tm_info->tm_year + 1900 == year);
}

// ------------------------------------------------------------
// 9.4 getMonthsWithEventsArray - Отримання масиву місяців, де є події
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// 9.5 getMonthsWithEvents - Отримання списку місяців з подіями у вигляді рядка
// ------------------------------------------------------------
String getMonthsWithEvents()
{
  int monthsArray[12];
  int monthCount = getMonthsWithEventsArray(monthsArray);

  if (monthCount == 0)
  {
    return "Немає подій";
  }

  String result = "";
  for (int i = 0; i < monthCount; i++)
  {
    if (i > 0)
      result += ", ";
    result += String(monthsArray[i]) + " - " + getMonthName(monthsArray[i]); // getMonthName нижче
  }
  return result;
}

// ============================================================
// БЛОК 10: ДОПОМІЖНІ ФУНКЦІЇ ФОРМАТУВАННЯ (НАЙНИЖЧИЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 10.1 formatTime - Форматування часу з мілісекунд у читабельний вигляд
// ------------------------------------------------------------
String formatTime(unsigned long ms)
{
  unsigned long sec = ms / 1000;
  int days = sec / 86400;
  int hours = (sec % 86400) / 3600;
  int minutes = (sec % 3600) / 60;
  int seconds = sec % 60;

  String result = "";
  if (days > 0)
    result += String(days) + "д ";
  if (hours > 0 || days > 0)
    result += String(hours) + "г ";
  result += String(minutes) + "хв " + String(seconds) + "с";
  return result;
}

// ------------------------------------------------------------
// 10.2 getMonthName - Отримання назви місяця українською мовою
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// 10.3 formatDateTime - Повне форматування дати та часу
// ------------------------------------------------------------
String formatDateTime(time_t timestamp)
{
  struct tm *tm_info = localtime(&timestamp);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", tm_info);
  return String(buffer);
}

// ------------------------------------------------------------
// 10.4 formatDateTimeShort - Коротке форматування дати та часу
// ------------------------------------------------------------
String formatDateTimeShort(time_t timestamp)
{
  struct tm *tm_info = localtime(&timestamp);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d.%m %H:%M", tm_info);
  return String(buffer);
}

// ============================================================
// БЛОК 11: ФУНКЦІЇ РОБОТИ З АПАРАТНИМ ЗАБЕЗПЕЧЕННЯМ (НАЙНИЖЧИЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 11.1 readVoltage - Читання напруги з аналогового входу
// ------------------------------------------------------------
float readVoltage()
{
  int raw = analogRead(POWER_PIN);
  return (raw / 1024.0 * VOLTAGE_REF);
}

// ------------------------------------------------------------
// 11.2 isPcOn - Визначення стану ПК з гістерезисом
// ------------------------------------------------------------
bool isPcOn()
{
  float voltage = readVoltage();

  if (voltage > VOLTAGE_ON)
  {
    return true;
  }
  else if (voltage < VOLTAGE_OFF)
  {
    return false;
  }
  return pcIsOn; // Гістерезис: повертаємо попередній стан
}

// ============================================================
// БЛОК 12: ФУНКЦІЇ РОБОТИ З ПОДІЯМИ (НИЖЧИЙ РІВЕНЬ)
// ============================================================

// ------------------------------------------------------------
// 12.1 addEventToLog - Додавання події в історію
// ------------------------------------------------------------
void addEventToLog(float voltage)
{
  if (eventCount < MAX_EVENTS)
  {
    eventLog[eventCount].timestamp = time(nullptr);
    eventLog[eventCount].state = pcIsOn;
    eventLog[eventCount].voltage = voltage;
    eventCount++;
  }
  else
  {
    // Якщо масив переповнений - зсуваємо всі події на 1 позицію вліво
    for (int i = 0; i < MAX_EVENTS - 1; i++)
    {
      eventLog[i] = eventLog[i + 1];
    }
    eventLog[MAX_EVENTS - 1].timestamp = time(nullptr);
    eventLog[MAX_EVENTS - 1].state = pcIsOn;
    eventLog[MAX_EVENTS - 1].voltage = voltage;
  }
}

// ------------------------------------------------------------
// 12.2 sendStateChangeNotification - Відправка сповіщення про зміну стану
// ------------------------------------------------------------
void sendStateChangeNotification(float voltage)
{
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

// ============================================================
// БЛОК 13: РОБОТА З ФАЙЛОВОЮ СИСТЕМОЮ (LITTLEFS) - НАЙНИЖЧИЙ РІВЕНЬ
// ============================================================

// ------------------------------------------------------------
// 13.1 saveData - Збереження всіх даних у JSON файл
// ------------------------------------------------------------
void saveData()
{
  StaticJsonDocument<4096> doc;

  doc["workTime"] = workTime;
  doc["onPcTime"] = onPcTime;
  doc["offPcTime"] = offPcTime;
  doc["eventCount"] = eventCount;

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

// ------------------------------------------------------------
// 13.2 loadData - Завантаження даних з JSON файлу
// ------------------------------------------------------------
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
    file.close();
    return;
  }

  workTime = doc["workTime"] | 0;
  onPcTime = doc["onPcTime"] | 0;
  offPcTime = doc["offPcTime"] | 0;
  eventCount = doc["eventCount"] | 0;

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
// БЛОК 14: ФУНКЦІЇ ІНІЦІАЛІЗАЦІЇ (SETUP)
// ============================================================

// ------------------------------------------------------------
// 14.1 initializeFileSystem - Ініціалізація файлової системи LittleFS
// ------------------------------------------------------------
void initializeFileSystem()
{
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
}

// ------------------------------------------------------------
// 14.2 initializePcState - Ініціалізація початкового стану ПК
// ------------------------------------------------------------
void initializePcState()
{
  float voltage = readVoltage();
  pcIsOn = (voltage > VOLTAGE_ON);
  prevPcState = pcIsOn;

  Serial.println("📊 Початкова напруга: " + String(voltage, 2) + "V");
  Serial.println("💻 Стан ПК: " + String(pcIsOn ? "УВІМКНЕНО" : "ВИМКНЕНО"));
  Serial.println("🕐 Поточний час: " + formatDateTime(time(nullptr)));
}

// ------------------------------------------------------------
// 14.3 addInitialEvent - Додавання першої події в історію
// ------------------------------------------------------------
void addInitialEvent()
{
  if (eventCount < MAX_EVENTS)
  {
    eventLog[eventCount].timestamp = time(nullptr);
    eventLog[eventCount].state = pcIsOn;
    eventLog[eventCount].voltage = readVoltage();
    eventCount++;
  }
}

// ------------------------------------------------------------
// 14.4 initializeTelegram - Ініціалізація Telegram бота
// ------------------------------------------------------------
void initializeTelegram()
{
  bot.attach(newMsg);
  delay(1000);
}

// ------------------------------------------------------------
// 14.5 sendStartupMessage - Відправка стартового повідомлення адміністратору
// ------------------------------------------------------------
void sendStartupMessage()
{
  Serial.println("\n✅ WEMOS: увімкнувся!");

  String startMsg = "✅ WEMOS: увімкнувся!\n";
  startMsg += "💻 ПК: " + String(pcIsOn ? "✅ працює" : "❌ не працює");
  startMsg += "\n📊 Напруга: " + String(readVoltage(), 2) + "V";
  startMsg += "\n🕐 " + formatDateTime(time(nullptr));

  bot.sendMessage(startMsg, CHAT_ID_ADMIN);
}

// ------------------------------------------------------------
// 14.6 initializeTimers - Ініціалізація всіх таймерів
// ------------------------------------------------------------
void initializeTimers()
{
  lastMillis = millis();
  lastSaveTime = millis();
  lastNtpUpdate = millis();
  inMonthMenu = false;
}

// ------------------------------------------------------------
// 14.7 setup - ГОЛОВНА ФУНКЦІЯ ІНІЦІАЛІЗАЦІЇ
// ------------------------------------------------------------
void setup()
{
  // ---- 1. Ініціалізація Serial ----
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n🚀 Запуск WEMOS...");
  delay(3000);

  // ---- 2. Ініціалізація файлової системи ----
  initializeFileSystem();

  // ---- 3. Налаштування пінів ----
  pinMode(POWER_PIN, INPUT);
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, HIGH);

  // ---- 4. Завантаження збережених даних ----
  loadData();

  // ---- 5. Підключення до Wi-Fi ----
  rowsWifiList = (sizeof(wifiList) / sizeof(wifiList[0]));
  connectWiFi();

  // ---- 6. Синхронізація часу ----
  syncTime();

  // ---- 7. Визначення початкового стану ПК ----
  initializePcState();

  // ---- 8. Додаємо першу подію в історію ----
  addInitialEvent();

  // ---- 9. Ініціалізація Telegram бота ----
  initializeTelegram();

  // ---- 10. Відправка привітального повідомлення ----
  sendStartupMessage();

  // ---- 11. Ініціалізація таймерів ----
  initializeTimers();
}