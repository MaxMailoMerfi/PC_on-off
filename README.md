Включення, виключення пк

Це через VS Code.



Розширення Arduino Maker Workshop.

Завантаження 

Щоб створити файли для праці і обновити [bin], натисніть [Complite] в розширенні  Arduino Maker Workshop.

Файл bin копіюється натисканням [Ctrl+Shift+B] та вибором [build+copy] і знаходиться поруч з ino.
це створений код (copy_bin.bat) в папці (.vscode)

Boards Manager esp8266
https://arduino.esp8266.com/stable/package_esp8266com_index.json
LOLIN(WeMos) D1 R1

Library Manager
Arduino_DebugUtils
Arduino_ESP32_OTA
Arduino_MachineControl
ArduinoHttpClient
ArduinoJson
ArduinoRS485
FastBot
FastLED
HttpClient
LittleFS_esp32
Rtc by Makuna
TFT_eSPI
WiFi


📌 Структура коду за блоками:
БЛОК 1: Конфігурація - всі налаштування та константи

БЛОК 2: Глобальні змінні - змінні, доступні в усій програмі

БЛОК 3: Допоміжні функції - readVoltage(), isPcOn(), formatTime()

БЛОК 4: Робота з файлами - saveData(), loadData()

БЛОК 5: Wi-Fi - connectWiFi()

БЛОК 6: Telegram - newMsg() - обробник повідомлень

БЛОК 7: Setup() - ініціалізація

БЛОК 8: Loop() - головний цикл