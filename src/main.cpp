#include <Arduino.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <WiFiSettings.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "certs.h"
#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include "TM1637Display.h"

void sendHttpsRequest(String temp, String humi);

String url = "/Collector";
X509List cert(cert_DigiCert_High_Assurance_EV_Root_CA);
StaticJsonDocument<256> json_doc;
char json_output[256];

// Use WiFiClientSecure class to create TLS connection
WiFiClientSecure client;

DHTesp dht;
#define REPORT_TIME 360 // 1hour
int iCntTime = 0;

#define CLK D0
#define DIO D2

TM1637Display display(CLK, DIO);

#define DIFFERENCE_TEMPERATURE 1.5
#define DIFFERENCE_HUMIDITY 5.0
float lastTemperature = 0.0;
float lastHumidity = 0.0;

bool bDataSend = false;
bool bScreenCleared = false;
const uint8_t SEG_CONN[] = {
    SEG_A | SEG_D | SEG_E | SEG_F, // C
    SEG_C | SEG_D | SEG_E | SEG_G, // o
    SEG_C | SEG_E | SEG_G,         // n
    SEG_C | SEG_E | SEG_G          // n
};

const uint8_t SEG_PASS[] = {
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,         // P
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,         // S
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G          // S
};

const uint8_t SEG_FAIL[] = {
    SEG_A | SEG_E | SEG_F | SEG_G,                 // F
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
    SEG_F | SEG_G,                                 // I
    SEG_D | SEG_F | SEG_G                          // L
};

const uint8_t SEG_DONE[] = {
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,         // d
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
    SEG_C | SEG_E | SEG_G,                         // n
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G          // E
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Vision Tempreture Monitor Ver1.0");
  LittleFS.begin();
  dht.setup(10, DHTesp::DHT22); // Connect DHT sensor to GPIO 2
  display.setBrightness(0x01);  // 0x01~0x0e
  display.setSegments(SEG_CONN);
  WiFiSettings.connect();

  // Set time via NTP, as required for x.509 validation
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  //  Serial.print("Connecting to ");
  //  Serial.println(github_host);

  // Serial.printf("Using certificate: %s\n", cert_DigiCert_High_Assurance_EV_Root_CA);
  client.setTrustAnchors(&cert);
  display.setSegments(SEG_PASS);
  // if (!client.connect(github_host, github_port))
  // {
  //   Serial.println("Connection failed");
  //   return;
  // }

  //  sendHttpsRequest("22", "52");
}

void sendHttpsRequest(String temp, String humi)
{
  // return;
  json_doc["Domain"] = "CheckTemperature";
  json_doc["Table"] = "TemperatureStatus";
  json_doc["Source"] = "溫度監控";
  json_doc["Data"][0]["Temperature"] = temp;
  json_doc["Data"][0]["Humidity"] = humi;
  serializeJson(json_doc, json_output);
  if (!client.connected())
  { //  reconnect to server
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2)
    {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
    if (!client.connect(github_host, github_port))
    {
      Serial.println("Connection failed");
      return;
    }
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);
  String msg = String(json_output);

  String req = String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + github_host + "\r\n" +
               "User-Agent: Vision Temperature Monitor\r\n" +
               "Content-Type: application/json" + "\r\n" +
               "Content-Length: " + msg.length() + "\r\n\r\n" +
               msg + "\r\n";

  // Serial.println(req);
  client.println(req);

  Serial.println("Request sent");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      Serial.println("Headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\""))
  {
    Serial.println("esp8266/Arduino CI successful!");
  }
  else
  {
    Serial.println("esp8266/Arduino CI has failed");
  }
  // Serial.println("Reply was:");
  // Serial.println("==========");
  //  Serial.println(line);
  // Serial.println("==========");
  // Serial.println("Closing connection");
}

void loop()
{
  iCntTime++;

  // delay(dht.getMinimumSamplingPeriod());

  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  String strTemp = String(temperature, 1);
  String strHumi = String(humidity, 1);
  if (bScreenCleared == false)
  {
    display.clear();
    bScreenCleared = true;
  }

  display.showNumberDecEx(int(temperature * 10), 0x40, false, 3, 0);

  if (iCntTime >= REPORT_TIME)
  {
    iCntTime = 0;
    Serial.println("Tiem to report.......");
    sendHttpsRequest(strTemp, strHumi);
  }

  Serial.print(strTemp);
  Serial.print("\t");
  Serial.println(strHumi);

  if (temperature != lastTemperature)
  {
    float error = abs(temperature - lastTemperature);
    Serial.println(error);

    if (error > DIFFERENCE_TEMPERATURE)
    {
      lastTemperature = temperature;
      sendHttpsRequest(strTemp, strHumi);
      Serial.println("Temperature changed");
      Serial.print(dht.getStatusString());
      Serial.print("\t");
      //    Serial.print(humidity, 1);
      //    Serial.print("\t\t");
      Serial.print(temperature, 1);
      Serial.print("\t");
      //    Serial.print(dht.toFahrenheit(temperature), 1);
      //    Serial.print("\t\t");
      //    Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
      //    Serial.print("\t\t");
      Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);
    }
  }

  if (humidity != lastHumidity)
  {
    float error = abs(humidity - lastHumidity);
    Serial.println(error);

    if (error > DIFFERENCE_HUMIDITY)
    {
      lastHumidity = humidity;
      sendHttpsRequest(strTemp, strHumi);
      Serial.println("Humidity changed");
      Serial.print(dht.getStatusString());
      Serial.print("\t");
      //    Serial.print(humidity, 1);
      //    Serial.print("\t\t");
      Serial.print(temperature, 1);
      Serial.print("\t");
      //    Serial.print(dht.toFahrenheit(temperature), 1);
      //    Serial.print("\t\t");
      //    Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
      //    Serial.print("\t\t");
      Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);
    }
  }

  delay(2500); // 4 seconds
}