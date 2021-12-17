
#define DHT_PIN 5

#define BTN_PIN 25

#define WIFI_SSID "TP-Link_Guest"
#define WIFI_PASSWORD ""

#define API_KEY ""
#define PROGRAM_IP ""

#define TIME_API_URL "http://worldtimeapi.org/api/timezone/Europe/Kiev"
#define CRYPTO_API_URL "https://api.coincap.io/v2/assets/"

#define MAX_CRYPTO_CURRENCIES 4


#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#include "DHT.h"

#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <TimeLib.h>
#include "GyverTimer.h"

#include "GyverButton.h"

/// Іконки
byte celsius[] = {B01000, B10100, B01000, B00110, B01000, B01000, B01000, B00110};
byte connection[] = {B00000, B00001, B00001, B00101, B00101, B10101, B10101, B10101};
byte connectionNone[] = {B00000, B00000, B00000, B00000, B00000, B00000, B10101, B10101};

/// Дисплей
LiquidCrystal_I2C lcd(0x27, 16, 2);

/// Датчик температури
DHT dht(DHT_PIN, DHT11);

/// Кнопка
GButton button(BTN_PIN, HIGH_PULL);

/// Таймер
GTimer updateTimer(MS, 10000); // Оновлення криптовалюти та температури на дисплеї


/* Змінні наслаштування */
boolean is12HourFormat = true;

short nowCryptoCurrency  = 0;
short lastCryptoCurrency = nowCryptoCurrency;

String cryptoCurrencies[MAX_CRYPTO_CURRENCIES] = {"bitcoin", "cardano"};
String cryptoCurrenciesCache[MAX_CRYPTO_CURRENCIES];


/// Відправити реквест
StaticJsonDocument<1000> httpGETRequest(String serverName) {

  StaticJsonDocument<1000> jsonResponse; // Json документ для отримання результатів реквестів

  HTTPClient client;

  if ((WiFi.status() == WL_CONNECTED)) {
    client.begin(serverName);

    short responseCode = client.GET();

    if (responseCode > 0) {
      switch (responseCode) {

      case 429:
        Serial.println("Too many requests");
      break;
      
      default:
        deserializeJson(jsonResponse, client.getStream());
      break;

      }
    } 
    else {
      Serial.println("Error on HTTP request");
    }

  } 
  else {
    Serial.println("Connection lost");
  }

  client.end();
  return jsonResponse;
}

/// Встановлюємо теперішній час на екран
void setNowTime() {
  int nowHour   = hour();
  int nowMinute = minute();
  
  String nowTime = ":";

  if (nowMinute < 10) {
    nowTime = nowTime + "0" + (String)nowMinute;
  } 
  else {
    nowTime = nowTime + (String)nowMinute;
  }

  if (is12HourFormat) {
    if (nowHour > 12) {
      nowHour = nowHour - 12;
    } 
    else if (nowHour == 00) {
      nowHour = 12;
    }
    if (nowHour < 10) {
      nowTime = "0" + (String)nowHour + nowTime;
    } 
    else {
      nowTime = (String)nowHour + nowTime;
    } 
  } 
  else {
    if (nowHour < 10) {
      nowTime = "0" + (String)nowHour + nowTime;
    } 
    else {
      nowTime = (String)nowHour + nowTime;
    }
  }

  lcd.setCursor(6,0);
  lcd.print(nowTime);
}

/// Встановлюємо валюту на екран
String getNowCrypto(String name) {
  StaticJsonDocument<1000> response = httpGETRequest(CRYPTO_API_URL + name);
  
  if (!response.isNull()) {
    
    String result = ":";

    result       = response["data"]["symbol"].as<String>() + result;
    float price  = response["data"]["priceUsd"].as<float>();

    if (price > 1000) {
      result = result + " " + (int)round(price);
    } 
    else {
      result = result + " " + (round(price * 100) / 100);
    }

    Serial.println(result);
    
    response.clear();
    return result;

  } 
  else {
    Serial.println("Error to get crypto price");

    response.clear();
    return "";
  }
}

void setCryptoOnDisplay(String cryptoResult) {
  cryptoCurrenciesCache[nowCryptoCurrency] = cryptoResult; // Запис результату в кеш

  short position = (16 - cryptoResult.length()) / 2;

  lcd.setCursor(0, 1);
  lcd.print("                ");

  lcd.setCursor(position, 1);
  lcd.print(cryptoResult);
}

void swapNowCrypto() {
  nowCryptoCurrency++;

  if (nowCryptoCurrency != MAX_CRYPTO_CURRENCIES - 1) {

    while (cryptoCurrencies[nowCryptoCurrency] == "") {
      nowCryptoCurrency++;

      if (nowCryptoCurrency == MAX_CRYPTO_CURRENCIES - 1) nowCryptoCurrency = 0;
    }

  } else {
    nowCryptoCurrency = 0;
  }
}


TaskHandle_t updateTask; // Змінна якою можна керувати 2 ядром

/// Функція яка виконується в 2 ядрі
void updateButtonTask(void * pvParameters){
  for(;;) {
    button.tick();

    if (button.isClick()) swapNowCrypto();
    delay(10);
  }
}


void setup(){
  Serial.begin(115200);

  /// Ініціалізація датвчика температури
  dht.begin(); 

  /// Ініціалізація дисплею
  lcd.init();  
  lcd.backlight();

  /// Ініціалізація іконок
  lcd.createChar(0, celsius);    // Градуси цельсія
  lcd.createChar(1, connection); // Підключено до програми
  lcd.createChar(2, connectionNone); // Не підключено до програми

  /// Меню завантаження годинника
  lcd.setCursor(2,0);
  lcd.print("Stera Watch");

  lcd.setCursor(3,1);
  lcd.print("Starting..");
  
  /// Ініціалізація вай фай
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");

  /// Ініціалізація часу
  StaticJsonDocument<1000> timeResponse = httpGETRequest(TIME_API_URL); // Отримуємо теперішній час

  if (!timeResponse.isNull()) {
    setTime(timeResponse["unixtime"].as<long>() + (2 * 60 * 60)); // Записуємо теперішній час + 2 години
  }
  timeResponse.clear();

  /// Отримання ціни криптовалюти
  String cryptoResult = getNowCrypto(cryptoCurrencies[nowCryptoCurrency]);

  while (cryptoResult == "") {
    if (cryptoResult != "") {
      break;
    }
    cryptoResult = getNowCrypto(cryptoCurrencies[nowCryptoCurrency]);
  }
  
  /// Отримання температури
  int temperature;
  
  if (!isnan(dht.readTemperature())) {
    temperature = (int)round(dht.readTemperature());
  }

  /* Ініціалізація 2 ядра */
  xTaskCreatePinnedToCore(
                    updateButtonTask,
                    "updateButton",
                    1024,
                    NULL,
                    1,
                    &updateTask,
                    0);

  delay(500);

  lcd.clear();

  /// Встановлення криптовалюти на екран
  setCryptoOnDisplay(cryptoResult);
  
  /// Встановлення температури на екран
  lcd.setCursor(0,0);
  lcd.print(temperature);

  /// Встановлення іконок на екран  
  lcd.setCursor(2,0);
  lcd.write(0);

  lcd.setCursor(15,0);
  lcd.write(2);
}

void loop() {
  setNowTime();

  if (lastCryptoCurrency != nowCryptoCurrency) {
    lastCryptoCurrency = nowCryptoCurrency;

    if (cryptoCurrenciesCache[nowCryptoCurrency] != "") {
      setCryptoOnDisplay(cryptoCurrenciesCache[nowCryptoCurrency]);
    } 
    else {
      lcd.setCursor(0,1);
      lcd.print("                ");
    }
  }

  /* Оновлення криптовалюти кожні 10с */
  if (updateTimer.isReady()) {
    Serial.print(ESP.getFreeHeap());
    
    /// Відобразити теперішню криптовалюту
    short momentCryptoCurrency = nowCryptoCurrency;
    String cryptoResult = getNowCrypto(cryptoCurrencies[momentCryptoCurrency]);

    if (cryptoResult != "" && momentCryptoCurrency == nowCryptoCurrency) {
      setCryptoOnDisplay(cryptoResult);
    } 
    
    /// Відобразити теперішню температуру
    if (!isnan(dht.readTemperature())) {
      lcd.setCursor(2,0);
      lcd.write(0);
      
      lcd.setCursor(0,0);
      lcd.print((int)round(dht.readTemperature()));
    }
  }
}
