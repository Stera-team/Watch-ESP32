
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#include "DHT.h"

#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <TimeLib.h>


#define DHT_PIN 5

#define WIFI_SSID "TP-Link_Guest"
#define WIFI_PASSWORD ""

#define API_KEY ""
#define PROGRAM_IP ""

#define TIME_API_URL "http://worldtimeapi.org/api/timezone/Europe/Kiev"
#define CRYPTO_API_URL "https://api.coincap.io/v2/assets/"

/// Іконки
byte celsius[] = {B01000, B10100, B01000, B00110, B01000, B01000, B01000, B00110};
byte connection[] = {B00000, B00001, B00001, B00101, B00101, B10101, B10101, B10101};

/// Дисплей
LiquidCrystal_I2C lcd(0x27, 16, 2);

/// Датчик температури
DHT dht(DHT_PIN, DHT11);

/// Json обєкт для отримання результатів реквестів
DynamicJsonDocument jsonResponse(1024);

/// Змінні наслаштування
boolean is12HourFormat = true;


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
  httpGETRequest(TIME_API_URL);                                 // Отримуємо теперішній час
  setTime(jsonResponse["unixtime"].as<long>() + (2 * 60 * 60)); // Записуємо теперішній час + 2 години

  /// Отримання ціни криптовалюти
  String cryptoResult = getNowCrypto("solana");
  int cryptoPos;

  while (cryptoResult == "0") {
    if (cryptoResult != "0") {
      break;
    }
    cryptoResult = getNowCrypto("solana");
  }
  cryptoPos = (16 - cryptoResult.length()) / 2;
  
  /// Отримання температури
  int temperature;
  
  if (!isnan(dht.readTemperature())) {
    temperature = (int)round(dht.readTemperature());
  }

  delay(500);

  lcd.clear();

  /// Встановлення криптовалюти на екран
  lcd.setCursor(cryptoPos, 1);
  lcd.print(cryptoResult);
  
  /// Встановлення температури на екран
  lcd.setCursor(0,0);
  lcd.print(temperature);

  /// Встановлення іконок на екран  
  lcd.setCursor(2,0);
  lcd.write(0);

  lcd.setCursor(15,0);
  lcd.write(1);
}

void loop() { 
  setNowTime();

  String cryptoResult = getNowCrypto("solana");

  if (cryptoResult != "0") {
    int position = (16 - cryptoResult.length()) / 2;

    lcd.setCursor(0, 1);
    lcd.print("                ");

    lcd.setCursor(position, 1);
    lcd.print(cryptoResult);
  } 
  
  if (!isnan(dht.readTemperature())) {
    lcd.setCursor(2,0);
    lcd.write(0);
    
    lcd.setCursor(0,0);
    lcd.print((int)round(dht.readTemperature()));
  }

  delay(1000);
}

/// Відправити реквест
bool httpGETRequest(String serverName) {
  if ((WiFi.status() == WL_CONNECTED)) {

    HTTPClient client;
    client.begin(serverName);

    if (client.GET() > 0) {
      if (client.GET() != 429) {
        deserializeJson(jsonResponse, client.getStream());

        client.end();
        return true;
      }
      else {
        Serial.println("Too many requests");
        
        client.end();
        return false;
      }
      
    } 
    else {
      Serial.println("Error on HTTP request");
      client.end();
    }
  } 
  else {
    Serial.println("Connection lost");
  }
  
  return false;
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
  } else {
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
  
  if (httpGETRequest(CRYPTO_API_URL + name)) {
    
    String result = ":";

    result      = jsonResponse["data"]["symbol"].as<String>() + result;
    float price = jsonResponse["data"]["priceUsd"].as<float>();

    if (price > 1000) {
      result = result + " " + (int)round(price);
    } 
    else {
      result = result + " " + (round(price * 100) / 100);
    }

    Serial.println(result);
    return result;

  } 
  else {
    Serial.println("Error to get crypto price");
    return "0";
  }
}
