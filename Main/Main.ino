
#define DHT_PIN 5
#define BTN_PIN 25
#define BUZZER_PIN 13

#define WIFI_SSID "TP-Link_Guest"
#define WIFI_PASSWORD ""

#define API_KEY "ROHZKDBN"
#define PROGRAM_IP "http://192.168.0.52:5000"

#define TIME_API_URL "http://worldtimeapi.org/api/timezone/Europe/Kiev"
#define CRYPTO_API_URL "https://api.coincap.io/v2/assets/"

#define MAX_CRYPTO_CURRENCIES 4
#define MAX_ALARMS 3

#define EEPROM_ADDR 0
#define EEPROM_SIZE 1024


#include <EEPROM.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#include "DHT.h"

#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <MD5.h>

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

StaticJsonDocument<1000> jsonResponse; // Json документ для отримання результатів реквестів

/// Таймери
GTimer updateTimer(MS, 10000); // Оновлення криптовалюти та температури на дисплеї
GTimer updateInfoFromBridge(MS, 3000); // Оновлення налаштувань якщо вони змінились
GTimer updateBuzzer(MS, 1000); // Оновлення пищання бузера

/* Змінні наслаштування */
boolean isConnectedToBridge = false;
boolean is12HourFormat = true;
boolean isNowAlarm = false;
boolean firstStart = false;

String nowProgram  = "mainLoop"; // Процес який зараз відбувається в Loop

short nowCryptoCurrency  = 0;
short lastCryptoCurrency = nowCryptoCurrency;

String cryptoCurrencies[MAX_CRYPTO_CURRENCIES] = {"bitcoin", "cardano"};
String cryptoCurrenciesCache[MAX_CRYPTO_CURRENCIES];

String alarmsNames[MAX_ALARMS] = {};
String alarmsTimes[MAX_ALARMS] = {};

String settingsHash;


/// Відправити реквест
void httpGETRequest(String serverName) {

  HTTPClient client;

  if ((WiFi.status() == WL_CONNECTED)) {
    client.begin(serverName);

    short responseCode = client.GET();

    if (responseCode > 0) {
      switch (responseCode) {

      case 429:
        Serial.println("Too many requests");
      break;

      case 102:
        Serial.println("Connection Refused");
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
  jsonResponse.clear();
  httpGETRequest(CRYPTO_API_URL + name);
  
  if (!jsonResponse.isNull()) {
    
    String result = ":";

    result       = jsonResponse["data"]["symbol"].as<String>() + result;
    float price  = jsonResponse["data"]["priceUsd"].as<float>();

    if (price > 1000) {
      result = result + " " + (int)round(price);
    } 
    else {
      result = result + " " + (round(price * 100) / 100);
    }

    Serial.println(result);
    
    jsonResponse.clear();
    return result;

  } 
  else {
    Serial.println("Error to get crypto price");

    jsonResponse.clear();
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

    if (!isNowAlarm) {
      if (button.isSingle()) swapNowCrypto();
      if (button.isTriple()) nowProgram = "connectingToBridge";

    } else {
       if (button.isDouble()) isNowAlarm = false;
    }
    

    delay(10);
  }
}

void setAlarmOnDisplay(short index) {
  boolean isNowAlarm = true;
  
  lcd.clear();

  setNowTime();

  lcd.setCursor((16 - alarmsNames[index].length()) / 2, 1);
  lcd.print(alarmsNames[index]);

  alarmsNames[index] = "";
  alarmsTimes[index] = "";

  boolean state = HIGH;

  while (isNowAlarm) {   
    if (updateBuzzer.isReady()) {
      digitalWrite(BUZZER_PIN, state);

      if (state == HIGH) {
        state = LOW;
      } else {
        state = HIGH;
      }
    }
  }
  digitalWrite(BUZZER_PIN, LOW);

  setMainMenuOnDisplay();
}

/// Встановлення початкового меню на дисплеї
void setMainMenuOnDisplay() {
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

  lcd.clear();

  /// Встановлення криптовалюти на екран
  setCryptoOnDisplay(cryptoResult);
  
  /// Встановлення температури на екран
  lcd.setCursor(0,0);
  lcd.print(temperature);

  /// Встановлення іконок на екран  
  lcd.setCursor(2,0);
  lcd.write(0);

  if (isConnectedToBridge) {
    lcd.setCursor(15,0);
    lcd.write(1);

  } else {
    lcd.setCursor(15,0);
    lcd.write(2);
  }
}

void showErrorBridgeIsntAvailable() {
  jsonResponse.clear();
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Bridge");

  lcd.setCursor(0,1);
  lcd.print("isn't available!");

  delay(3000);

  nowProgram = "mainLoop";
  setMainMenuOnDisplay();
}

void setNowSettings() {
  is12HourFormat = jsonResponse["settings"]["is12HourFormat"].as<boolean>();
  settingsHash   = jsonResponse["settingsHash"].as<String>();

  memset(cryptoCurrencies, 0, sizeof(cryptoCurrencies));
  memset(cryptoCurrenciesCache, 0, sizeof(cryptoCurrenciesCache));

  if (jsonResponse["settings"]["cryptoTickers"].size() == 0) {
    cryptoCurrencies[0] = "bitcoin";
  }

  for(short i = 0; i < jsonResponse["settings"]["cryptoTickers"].size(); i++) {
    cryptoCurrencies[i] = jsonResponse["settings"]["cryptoTickers"][i].as<String>();
  }

  if (jsonResponse["settings"]["alarms"].size() == 0) {
    memset(alarmsNames, 0, sizeof(alarmsNames));
    memset(alarmsTimes, 0, sizeof(alarmsTimes));

  } else {
    for(short i = 0; i < jsonResponse["settings"]["alarms"].size(); i++) {
      alarmsNames[i] = jsonResponse["settings"]["alarms"][i]["name"].as<String>();
      alarmsTimes[i] = jsonResponse["settings"]["alarms"][i]["time"].as<String>();
    }
  }
  
}

String createNowSettingsJson() {
  StaticJsonDocument<512> nowSettingsJson;
  String result;

  nowSettingsJson["is12HourFormat"].set(is12HourFormat);

  nowSettingsJson.createNestedArray("cryptoTickers");
  nowSettingsJson.createNestedArray("alarms");

  for (short i = 0; i < MAX_CRYPTO_CURRENCIES; i++) {
    if (cryptoCurrencies[i] == "") break;

    nowSettingsJson["cryptoTickers"][i].set(cryptoCurrencies[i]);
  }

  for (short i = 0; i < MAX_ALARMS; i++) {
    if (alarmsNames[i] != "") {   
      nowSettingsJson["alarms"][i]["name"].set(alarmsNames[i]);
      nowSettingsJson["alarms"][i]["time"].set(alarmsTimes[i]);
    }
  }

  serializeJson(nowSettingsJson, result);
  nowSettingsJson.clear();

  return result;
}

void saveSettingsToEEPROM() {
  StaticJsonDocument<512> nowSettingsJson;
  String result;

  nowSettingsJson["settings"]["is12HourFormat"].set(is12HourFormat);
  nowSettingsJson["settingsHash"].set(settingsHash);

  nowSettingsJson["settings"].createNestedArray("cryptoTickers");
  nowSettingsJson["settings"].createNestedArray("alarms");

  for (short i = 0; i < MAX_CRYPTO_CURRENCIES; i++) {
    if (cryptoCurrencies[i] == "") break;

    nowSettingsJson["settings"]["cryptoTickers"][i].set(cryptoCurrencies[i]);
  }

  for (short i = 0; i < MAX_ALARMS; i++) {
    if (alarmsNames[i] != "") {   
      nowSettingsJson["alarms"][i]["name"].set(alarmsNames[i]);
      nowSettingsJson["alarms"][i]["time"].set(alarmsTimes[i]);
    }
  }

  serializeJson(nowSettingsJson, result);
  nowSettingsJson.clear();

  int len = result.length();
  EEPROM.write(EEPROM_ADDR, len);

  for (int i = 0; i < len; i++) {
    EEPROM.write(EEPROM_ADDR + 2 + i, result[i]);
  }

  EEPROM.commit();
}

void setSettingsFromEEPROM() {
  int newStrLen = EEPROM.read(EEPROM_ADDR);
  char data[newStrLen + 1];

  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(EEPROM_ADDR + 2 + i);
  }
  data[newStrLen] = '\0';

  jsonResponse.clear();

  deserializeJson(jsonResponse, data);
  setNowSettings();

  jsonResponse.clear();
}

/// Підєднання до бріджа
void connectToBridge() {
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Connecting to");

  lcd.setCursor(0,1);
  lcd.print("Stera Bridge");
  
  jsonResponse.clear();
  httpGETRequest(((String)PROGRAM_IP) + "/Connect?apiKey=" + ((String)API_KEY));
  
  delay(2000);
  lcd.clear();

  if(!jsonResponse.isNull()) {

    if(jsonResponse["status"].as<boolean>()) {

      String randNumber    = ((String)random(100000, 999999));
      String randNumberStr = randNumber + ":" + ((String)API_KEY);

      char randNumberData[randNumberStr.length() + 1];
      randNumberStr.toCharArray(randNumberData, randNumberStr.length() + 1);

      unsigned char* hash = MD5::make_hash(randNumberData);
      String md5str       = MD5::make_digest(hash, 16);

      Serial.println(md5str);

      jsonResponse.clear();
      httpGETRequest(((String)PROGRAM_IP) + "/SendCheckCode?apiKey=" + ((String)API_KEY) + "&codeHash=" + md5str);

      if (!jsonResponse.isNull() && jsonResponse["status"].as<boolean>()) {
        lcd.setCursor(0,0);
        lcd.print("Check code:");

        lcd.setCursor(0,1);
        lcd.print("     " + randNumber + "     ");

        boolean waitingFor = true;
        jsonResponse.clear();

        while (waitingFor) {
          jsonResponse.clear();
          httpGETRequest(((String)PROGRAM_IP) + "/GetStatus?apiKey=" + ((String)API_KEY));

          if (!jsonResponse.isNull()) {

            if (jsonResponse["status"].as<String>() == "Connected") {
              /// Відправити всі настройки на брідж
              Serial.println("Connected to bridge");

              lcd.clear();

              lcd.setCursor(0,0);
              lcd.print("Connected");

              lcd.setCursor(0,1);
              lcd.print("to bridge");

              delay(3000);

              String nowSettings = createNowSettingsJson();

              char nowSettingsData[nowSettings.length() + 1];
              nowSettings.toCharArray(nowSettingsData, nowSettings.length() + 1);

              unsigned char* hash = MD5::make_hash(nowSettingsData);
              settingsHash        = MD5::make_digest(hash, 16);

              Serial.println(settingsHash);
              Serial.println(nowSettings);

              jsonResponse.clear();
              httpGETRequest(((String)PROGRAM_IP) + "/SetNowSettings?apiKey=" + ((String)API_KEY) + "&data=" + nowSettings);

              if (!jsonResponse.isNull() && jsonResponse["status"].as<boolean>()) {
                nowProgram = "mainLoop";
                waitingFor = false;
                isConnectedToBridge = true;

                setMainMenuOnDisplay(); 

              } else {
                lcd.clear();

                lcd.setCursor(0,0);
                lcd.print("Error while");

                lcd.setCursor(0,1);
                lcd.print("sending settings");

                delay(3000);

                jsonResponse.clear();
                waitingFor = false;
                nowProgram = "mainLoop";

                setMainMenuOnDisplay(); 
              }  

            } else if (jsonResponse["status"].as<String>() == "Founded") {
              Serial.println("Waiting for bridge");
              delay(2000);

            } else if (jsonResponse["status"].as<String>() == "Not Connected") {
              lcd.clear();

              lcd.setCursor(0,0);
              lcd.print("You entered");

              lcd.setCursor(0,1);
              lcd.print("incorrect code!");

              delay(3000);

              jsonResponse.clear();
              waitingFor = false;
              nowProgram = "mainLoop";

              setMainMenuOnDisplay();
            }

          } else {
            waitingFor = false;
            jsonResponse.clear();

            showErrorBridgeIsntAvailable();
          }
        }
        
      } else {
        jsonResponse.clear();
        showErrorBridgeIsntAvailable();
      }

    } else {
      lcd.setCursor(0,0);
      lcd.print("Incorrect");

      lcd.setCursor(0,1);
      lcd.print("Api key!");

      delay(3000);
      jsonResponse.clear();
 
      nowProgram = "mainLoop";
      setMainMenuOnDisplay();
    }

  } else {
    showErrorBridgeIsntAvailable();
  }
  
}


void setup(){
  EEPROM.begin(EEPROM_SIZE);

  pinMode(BUZZER_PIN, OUTPUT);

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

  if (!firstStart) {
    setSettingsFromEEPROM();
  }
  
  /// Ініціалізація вай фай
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");

  /// Ініціалізація часу
  while (jsonResponse.isNull()) {
    jsonResponse.clear();
    httpGETRequest(TIME_API_URL); // Отримуємо теперішній час
  }

  setTime(jsonResponse["unixtime"].as<long>() + (2 * 60 * 60)); // Записуємо теперішній час + 2 години
  jsonResponse.clear();


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

  setMainMenuOnDisplay(); // Встановлення початкового меню на дисплеї
}

void loop() {

  if (nowProgram == "mainLoop") {
    setNowTime();

    /* Перевірка будильників */
    String nowTime = ((String)hour()) + ":" + ((String)minute());

    for (short i = 0; i < MAX_ALARMS; i++) {
      if (alarmsTimes[i] == nowTime) {
        setAlarmOnDisplay(i);
      }
    }

    /* Перевірка чи є кеш цієї крипти поки вона не оновилась */
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

    /* Оновлення криптовалюти та температури кожні 10с */
    if (updateTimer.isReady()) {
      Serial.println(createNowSettingsJson());
      
      short momentCryptoCurrency = nowCryptoCurrency; // Запис криптовалюти на момент реквесту

      if (cryptoCurrencies[momentCryptoCurrency] != "") {
        String cryptoResult = getNowCrypto(cryptoCurrencies[momentCryptoCurrency]);

        /// Якщо результат нічого або під час реквесту змінилась криптовалюта на екрані
        if (cryptoResult != "" && momentCryptoCurrency == nowCryptoCurrency) {
          setCryptoOnDisplay(cryptoResult);
        }
      } else {
        lcd.setCursor(0, 1);
        lcd.print("                ");
      }
      
      /// Відобразити теперішню температуру
      if (!isnan(dht.readTemperature())) {
        lcd.setCursor(2,0);
        lcd.write(0);
        
        lcd.setCursor(0,0);
        lcd.print((int)round(dht.readTemperature()));
      }
    }

    if (isConnectedToBridge && updateInfoFromBridge.isReady()) {
      jsonResponse.clear();
      httpGETRequest(((String)PROGRAM_IP) + "/GetNowSettings?apiKey=" + ((String)API_KEY));

      if (!jsonResponse.isNull()) {
        if (jsonResponse["status"].as<boolean>() && settingsHash != jsonResponse["settingsHash"].as<String>()) {
          setNowSettings();
          jsonResponse.clear();

          saveSettingsToEEPROM();
        }
        
      } else {
        jsonResponse.clear();
        isConnectedToBridge = false;

        Serial.println("Bridge isnt available!");

        lcd.setCursor(15,0);
        lcd.write(2);
      }
    }


  } else if (nowProgram == "connectingToBridge") {
    connectToBridge();
  }

}
