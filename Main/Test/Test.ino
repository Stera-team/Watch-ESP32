#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#include "WiFi.h"
#include <HTTPClient.h>

#include <ArduinoJson.h>

//#include "DHT.h"
//
//DHT dht(16, DHT11);

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "TP-Link_Guest";
const char* password =  "";

byte celsius[] = {
  B01000,
  B10100,
  B01000,
  B00110,
  B01000,
  B01000,
  B01000,
  B00110
};

byte connection[] = {
  B00000,
  B00001,
  B00001,
  B00101,
  B00101,
  B10101,
  B10101,
  B10101
};

DynamicJsonDocument jsonResponse(500);

void setup() {

  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
	
	lcd.begin();
//  dht.begin();
  
  lcd.createChar(0, celsius);
  lcd.createChar(1, connection);
	
	lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("12");

  lcd.setCursor(2,0);
  lcd.write(0);
  
  lcd.setCursor(6,0);
	lcd.print("12:00");

  lcd.setCursor(15,0);
  lcd.write(1);

  lcd.setCursor(3,1);
  lcd.print("BTC: 67101");
}

void loop() {
//  lcd.setCursor(0,0);
//  lcd.print(round(dht.readTemperature()));

    HTTPClient http1;
    HTTPClient http2;
    
    http1.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http2.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String server1Path = "https://api.coincap.io/v2/assets/bitcoin";   
    http1.begin(server1Path.c_str());
    
    int httpResponseCode = http1.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      String payload = http1.getString();
      Serial.println(payload);

      deserializeJson(jsonResponse, payload);
      int price = round(jsonResponse["data"]["priceUsd"]);
      Serial.println(price);
      
      lcd.setCursor(8,1);
      lcd.print(price);
      
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http1.end();

    String server2Path = "http://worldtimeapi.org/api/timezone/Europe/Kiev";
    http2.begin(server2Path.c_str());
    
    httpResponseCode = http2.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      String payload = http2.getString();
      Serial.println(payload);

      deserializeJson(jsonResponse, payload);
      String nowTime = jsonResponse["datetime"];
      nowTime = nowTime.substring(11, 16);
      
      Serial.println(nowTime);
      
      lcd.setCursor(6,0);
      lcd.print(nowTime);
      
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http2.end();
}




//#include <Wire.h>
//#include <Adafruit_BMP280.h>
// 
//Adafruit_BMP280 bmp280;
// 
//void setup() {
//  Serial.begin(9600);
//  Serial.println(F("BMP280"));
// 
//  while (!bmp280.begin(BMP280_ADDRESS - 1)) {
//    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
//    delay(2000);
//  }
//}
// 
//void loop() {
//  float temperature = bmp280.readTemperature();
//  float pressure = bmp280.readPressure();
//  float altitude = bmp280.readAltitude(1013.25);
// 
//  Serial.print(F("Temperature = "));
//  Serial.print(temperature);
//  Serial.println(" *C");
// 
//  Serial.print(F("Pressure = "));
//  Serial.print(pressure);
//  Serial.println(" Pa");
// 
//  Serial.print(F("Altitude = "));
//  Serial.print(altitude);
//  Serial.println(" m");
// 
//  Serial.println();
//  delay(2000);
//}

//
//void setup() {
//  pinMode(4, INPUT);
//  Serial.begin(9600);
//}
//
//void loop() {
//  Serial.println(analogRead(4));
//  delay(500);
//}



#include "DHT.h"

#define DHTPIN 5

DHT dht(DHTPIN, DHT11);

void setup() {
  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));

  dht.begin();
}

void loop() {
  // Wait a few seconds between measurements.
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
}
