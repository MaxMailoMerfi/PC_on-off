// Не борюсь з фантомними повідомленнями, бо відключив лайки
String botVersion = " Версія 4.1";

// для прошивки конкретного бота зняти/поставити коментар

/*
#define WIFI_SSID "MERCUSYS_781C" // Бібліотека
#define WIFI_PASS "12345678"
#define BOT_TOKEN "6228832321:AAFYIvM7i-RkT0nVour_n2ciPYFkNhl4Swg" // Центр
String myNameBot = "Центр_бот";
#define relePin  0  // номер контакту для підключення реле
*/



#define WIFI_SSID "UKrtelecom_76NW7J" // Заготконтора
#define WIFI_PASS "0674840855"
#define BOT_TOKEN "6212826648:AAHL173v7dsOs32_Bqmz0pm-KceqaYmncKk" // Захід
#define relePin   0 // номер контакту для підключення реле
String myNameBot = "Захід_бот";


/*
#define WIFI_SSID "Tenda" // Міллер
#define WIFI_PASS "44332211"
#define BOT_TOKEN "5763958107:AAEIkp7zDWKYxEwhqlu_YPERhSdWDzClmvk" // Схід
String myNameBot = "Схід_бот";
#define relePin  0  // номер контакту для підключення реле
*/


#define CHAT_ID "-1001918614087" // ID TEST chanel
#define LONG_INTERVAL 5*60*1000 // 5 хвилин
#define SHORT_INTERVAL 20*1000 // 20 секунд



#include <FastBot.h>                      //https://github.com/GyverLibs/FastBot
FastBot bot(BOT_TOKEN);

int releStatus = 0;                       // стан реле 
unsigned long previousMillis;             // для заміру часу роботи сирени
unsigned long currentMillis;
unsigned long interval;                   // термін роботи сирени в мілісекундах

// Ключова фраза для пошуку в повідомлені
//String myStr1 = "Повітряна";              
//String myStr2 = "ВІДБІЙ";               
String myStr1 = "Повітряна тривога!!";    
String myStr2 = "ВІДБІЙ повітряної тривоги";

void setup() {
  //EEPROM.begin(4);   // minimum 4 байта
  Serial.begin(115200);
  connectWiFi();                          // підключаємося до мережі
  bot.sendMessage(myNameBot + " увімкнувся!" + botVersion, CHAT_ID);
  bot.attach(newMsg);                     // підключаємо функцію - обробник повідомлень

  pinMode(relePin, OUTPUT);               // налаштовуємо контакт для реле як вихід
  digitalWrite(relePin, HIGH);            // вимикаємо реле
  pinMode(LED_BUILTIN, OUTPUT);           // налаштовуємо контакт для світлодіода як вихід
  delay(10);
}

// Обробник повідомлень
void newMsg(FB_msg& msg) {

  uint32_t curentTime, messageTime;
  String msgText = msg.text;              // із структури повідомлення виділяємо сам текст
  String from_name = msg.username;        // із структури повідомлення виділяємо хто надіслав
  if (from_name == "")
    from_name = "Аноним";

  int32_t msgID = msg.messageID;  // ID сообщения


  // обновить, если файл имеет нужную подпись
  if (msg.OTA && msg.text == myNameBot ) bot.update();

  curentTime = bot.getUnix();     // поточний час в ЮНІКС форматі
  messageTime = msg.unix - 2;     // зменшимо час повідомлення щоб уникнути глюк
 
  //bot.sendMessage(myNameBot + ": msgID=" + msgID, CHAT_ID);


  // шукаємо ключову фразу "Повітряна тривога!"
  if (msgText.indexOf(myStr1) != -1) {
    interval = LONG_INTERVAL;
    bot.sendMessage(myNameBot + ": msgID=" + msgID + ": Отримано повідомлення про повітряну тривогу!", CHAT_ID);
    digitalWrite(relePin, LOW);     // вмикаємо реле
    digitalWrite(LED_BUILTIN, LOW);
    releStatus = 1;
    previousMillis = millis();      // запускаємо таймер
    return;
  }

  // шукаємо ключову фразу "ВІДБІЙ повітряної тривоги"
  if (msgText.indexOf(myStr2) != -1) {
    // Перевіряємо, чи не застаріло повідомлення
    if (curentTime - messageTime > 30) { // якщо повідомленню більше 30 секунд
      releStatus = 0;
      digitalWrite(relePin, HIGH);       // вимикаємо реле
      digitalWrite(LED_BUILTIN, HIGH);
      // ігноруємо це повідомлення
      bot.sendMessage(myNameBot + ": Повідомлення про відбій застаріло!", CHAT_ID);
      return;       
    }

    // повідомлення актуальне
    bot.sendMessage(myNameBot + ": msgID=" + msgID + ": Повідомлення про відбій актуальне! Вмикаю відбій", CHAT_ID);
    interval = SHORT_INTERVAL;
    digitalWrite(relePin, LOW);           // вмикаємо реле
    digitalWrite(LED_BUILTIN, LOW);
    releStatus = 1;
    previousMillis = millis();            // запускаємо таймер
    return;
  }

  // обробляємо натискання софт-кнопки в юзер меню в месенджері

  if (msgText == "Увімкнути сирену")  {
    interval = LONG_INTERVAL; // 5 хвилин
    digitalWrite(relePin, LOW);             // вмикаємо реле
    digitalWrite(LED_BUILTIN, LOW);
    releStatus = 1;
    previousMillis = millis();
    bot.sendMessage("Сирена увімкнена", msg.chatID);
  }

  if (msgText == "Вимкнути сирену")  {
    digitalWrite(relePin, HIGH);            // вимикаємо реле
    digitalWrite(LED_BUILTIN, HIGH);
    releStatus = 0;
    bot.sendMessage("Сирена вимкнена", msg.chatID);
  }

  if (msgText == "Стан сирени")  {
    if (releStatus) bot.sendMessage(myNameBot + ": Сирена виє.", msg.chatID);
    else bot.sendMessage(myNameBot + ": Сирена не виє.", msg.chatID);
  }

  if (msgText == "Аптайм")
  {
    unsigned long myTime = millis();
    int mymin = myTime / 1000 / 60;
    int myhour = mymin / 60;
    int myday = myhour / 24;

    String msg1 = String(mymin % 60);
    String msg2 = String(myhour % 24);
    String msg3 = String(myday);
    String msg0 = String(myNameBot + " працює " + msg3 + " діб " + msg2 + " годин " + msg1 + " хвилин");
    bot.sendMessage(msg0, msg.chatID);
  }

  if (msgText == "/start")         // формуємо юзер меню з софт кнопками
  {
    //String welcome = "Вітаю, " + from_name + ".\n";
    bot.sendMessage("Вітаю, " + from_name + ".\n", msg.chatID);

    // показати юзер меню (\t - горизонтальний поділ кнопок, \n - вертикальний
    bot.showMenu("Увімкнути сирену \t Стан сирени \t Вимкнути сирену", msg.chatID);
  }

  if (msgText == "Ресет") {
    bot.tickManual();
    bot.sendMessage(myNameBot +": Перезавантажуюсь.....", msg.chatID);
    ESP.restart();
  }
}

// основний цикл
void loop() {
  bot.tick();                               // опитуємо бот
  currentMillis = millis();

  // перевіряємо чи не пройшов час вимкнути сирену
  if (currentMillis - previousMillis >= interval)
  {
    releStatus = 0;
    digitalWrite(relePin, HIGH);            // вимикаємо реле
    digitalWrite(LED_BUILTIN, HIGH);
    //bot.sendMessage(myNameBot + ": вимкнув сирену", CHAT_ID);
  }
  //якщо минуло 3 доби, то перезавантажимося
  if (millis() / 1000 / 60 / 60 > 71  && releStatus == 0)   {
    bot.sendMessage(myNameBot + ": працюю три доби. Перевантажуюсь...", CHAT_ID);
    ESP.restart();
  }
}

void connectWiFi() {
  delay(2000);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() > 15000) ESP.restart();
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Connected");
}
