/*
     Погода в доме
     Мониторинг CO2, PM2.5, температуры и влажности с помощью ESP32 и Blynk
     
     zapimir@zapimir.net

     Licensed under MIT license
*/

#define BLYNK_PRINT Serial
#define BLYNK_HEARTBEAT 30
//#define BLYNK_DEBUG
#include "WiFiSettings.h"
#include <Wire.h>
#include <SPIFFS.h>

#include <BlynkSimpleEsp32.h>
#include "SoftwareSerial.h"
#include <forcedClimate.h>
#include <PMserial.h>

// Sensors config
// 0xFF probably means a bad address, a BMP 180 or BMP 085
// 0x56-0x58 represents a BMP 280
// 0x60 represents a BME 280
// 0x61 represents a BME 680
ForcedClimate climateSensor = ForcedClimate(Wire, 0x76);
SerialPM pms(PMSA003, 34, 35);  // PMSx003, RX, TX
SoftwareSerial s8(16, 17);      //Sets up a virtual serial port

float temperature, humidity, pressure;
BlynkTimer tempTimer;
BlynkTimer co2Timer;
BlynkTimer pmsTimer;

byte readCO2[] = { 0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25 };  //Command packet to read Co2
byte response[] = { 0, 0, 0, 0, 0, 0, 0 };                      //create an array to store the response

String getConfig(const String& fn) {
  File f = SPIFFS.open(fn, "r");
  String r = f.readString();
  f.close();
  return r;
}

String auth;

void setup() {
  Serial.begin(9600);
  Serial.println("Starting...");
  SPIFFS.begin(true);  // Will format on the first run after failing to mount
                       //SPIFFS.remove("/blynk-token");

  String ssid = getConfig("/wifi-ssid");
  String pass = getConfig("/wifi-password");
  auth = getConfig("/blynk-token");
  IPAddress host = IPAddress(51, 83, 250, 120);
  uint16_t port = 8080;

  if (!auth.length()) {
    auth = WiFiSettings.string("blynk-token", "", "Blynk Auth Token");  // Get token if stored
    WiFiSettings.portal();
  }
  Blynk.begin(auth.c_str(), ssid.c_str(), pass.c_str(), host, port);

  s8.begin(9600);
  climateSensor.begin();
  pms.init();  

  Serial.println("Started successfully");
  
  tempTimer.setInterval(1000L, takeTempMeasurements);
  co2Timer.setInterval(1000L, takeCO2Measurements);
  pmsTimer.setInterval(1000L, takePmsMeasurements);
}

void loop() {
  Blynk.run();
  tempTimer.run();
  co2Timer.run();
  pmsTimer.run();
}

void takeTempMeasurements() {
  climateSensor.takeForcedMeasurement();
  temperature = climateSensor.getTemperatureCelcius();
  humidity = climateSensor.getRelativeHumidity();
  pressure = climateSensor.getPressure() / 1.33322F;

  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, round(pressure));
}

void takeCO2Measurements() {
  bool status = sendRequest(readCO2);

  if (status) {
    Blynk.virtualWrite(V8, getValue(response));
  }
}

void takePmsMeasurements() {
  pms.read();
  if (pms) {
    Blynk.virtualWrite(V4, pms.pm01);
    Blynk.virtualWrite(V5, pms.pm25);
    Blynk.virtualWrite(V6, pms.pm10);
  }
}

bool sendRequest(byte packet[]) {
  int timeout = 0;
  while (!s8.available())  //keep sending request until we start to get a response
  {
    timeout++;
    if (timeout > 10)  //if it takes to long there was probably an error
      return false;
    s8.write(readCO2, 7);
    delay(50);
  }

  timeout = 0;            //set a timeoute counter
  while (s8.available() < 7)  //Wait to get a 7 byte response
  {
    timeout++;
    if (timeout > 10)  //if it takes to long there was probably an error
    {
      while (s8.available())  //flush whatever we have
        s8.read();
      return false;  //exit and try again
    }
    delay(50);
  }

  for (int i = 0; i < 7; i++) {
    response[i] = s8.read();
  }
  return true;
}

unsigned long getValue(byte packet[]) {
  int high = packet[3];  //high byte for value is 4th byte in packet in the packet
  int low = packet[4];   //low byte for value is 5th byte in the packet

  unsigned long val = high * 256 + low;  //Combine high byte and low byte with this formula to get value
  return val;
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}
