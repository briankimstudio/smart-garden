/*
  Smart garden
  2019/12/31
  Board
   Wemods D1 mini board

  PIN map
   A0 : Soil moisture
   D1 : SCL(I2C) BH1750
   D2 : SDA
   D5 : DHT11 Temp/humid
*/

#include <Wire.h>
#include <BH1750.h>
#include "DHT.h"
#include <TimeLib.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>  
  #include <ESP8266mDNS.h>
#elif defined (ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #define LED_BUILTIN 2
  #define D5 5
#endif
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define TIMEOUT          7000
#define VERBOSELN Serial.println
#define VERBOSE   Serial.print

int sensorPin = A0; 
int sensorValue;  
int maxValue;
int minValue;
int limit = 300; 

unsigned long previousMillis = 0;
const long interval = 60000;

// Replace with your SSID and Password
const char* ssid     = "ITM_332";
const char* password = "92221550";

// Replace with your unique Thing Speak WRITE API KEY
const char* cloudUrl="api.thingspeak.com";
const char* cloudKey="LANDXN0Z8XW2C76F";
const char* cloudChannel="941576";

String sysName="smart-garden";

#define DHTPIN D5
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);
  Wire.begin();
  dht.begin();
  lightMeter.begin();
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  minValue = analogRead(sensorPin);
  maxValue = minValue;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 
  int timeout = 10 * 4; // 10 seconds
  while(WiFi.status() != WL_CONNECTED  && (timeout-- > 0)) {
    delay(250);
    Serial.print(".");
  }  
  Serial.print("WiFi connected in: "); 
  Serial.print(millis());
  Serial.print(", IP address: "); 
  Serial.print(WiFi.localIP());  
  Serial.print(", MAC address: ");
  Serial.println(WiFi.macAddress());
  String macAddress = WiFi.macAddress();
  macAddress.toLowerCase();
  sysName += "-"+macAddress.substring(15);
  // OTA
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  Serial.println(sysName);
  ArduinoOTA.setHostname(sysName.c_str());
  
  ArduinoOTA.begin();

  Serial.println("Ready");
//  Serial.print("IP address: ");
//  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, LOW); 
  retrieveTime();    
//  delay(2000);
}

// the loop function runs over and over again forever
void loop() {
  unsigned long currentMillis = millis();  

  ArduinoOTA.handle();
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  //  delay(2000);                       // wait for a second
  //  delay(1000);                       // wait for a second

  char datetime_str[25];
  sprintf(datetime_str,"%4d-%02d-%02dT%02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
  Serial.print(datetime_str);
  Serial.print(" ");
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));

  sensorValue = analogRead(sensorPin); // Soil moisture

  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.print(" lx ");

  maxValue = sensorValue > maxValue ? sensorValue : maxValue;
  minValue = sensorValue < minValue ? sensorValue : minValue;
  Serial.print("Soil Moisture : ");
  Serial.print(minValue);
  Serial.print(" ");
  Serial.print(maxValue);
  Serial.print(" ");
  Serial.println(sensorValue);
 
//  if (sensorValue<limit) {
//    digitalWrite(LED_BUILTIN, HIGH); 
//  }
//  else {
//    digitalWrite(LED_BUILTIN, LOW); 
//  }

  if (currentMillis - previousMillis >= interval) {
    digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    //
    // temp, humid, light, soil
    sync(t, h, lux, sensorValue);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  }

  //////////////////////////////////////
  // Avoid delay() for OTA
  // OTA won't work during long delay()
  //////////////////////////////////////

 delay(100);
}

int sync(float temperature, float humidity, uint16_t light, int soil) {
  WiFiClient* client = new WiFiClient;
  char datetime_str[25];
  String postStr = "write_api_key=" + String(cloudKey) +"&time_format=absolute&updates=";
  sprintf(datetime_str,"%4d-%02d-%02dT%02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
  // 2018-06-14T12:12:22-0500
  postStr += String(datetime_str)+
    "+0800,"+
    String(temperature)+","+
    String(humidity)+","+
    String(light)+","+
    String(soil);

  VERBOSELN( postStr );
  
  if ( !client ) { return false; }

  if (client->connect( cloudUrl , 80 )) {
    client->println( "POST /channels/"+String(cloudChannel)+"/bulk_update.csv HTTP/1.1" );
    client->println( "Host: api.thingspeak.com" );
    client->println( "Connection: close" );
    client->println( "Content-Type: application/x-www-form-urlencoded" );
    client->println( "Content-Length: " + String( postStr.length() ) );
    client->println();
    client->println( postStr );

    String response;

    long startTime = millis();

    delay( 200 );
    while ( client->available() < 1 && (( millis() - startTime ) < TIMEOUT ) ){
      delay( 5 );
    }
    
  if( client->available() > 0 ){ // Get response from server.
   char charIn;
   do {
         charIn = client->read(); // Read a char from the buffer.
         response += charIn;     // Append the char to the string response.
       } while ( client->available() > 0 );
     }
     client->stop();
     
     if ( !response.indexOf("202 Accepted") ) {
      VERBOSELN("NM : sync : ERROR POST failed");
    } else {
      VERBOSELN("NM : sync : OK");
    }
  } else {
    VERBOSELN ( "NM : sync : ERROR Connection failed" );  
    return false;
  }
} 

int retrieveTime() {
  DynamicJsonDocument doc(1024);
  HTTPClient http;  //Declare an object of class HTTPClient
  int httpCode = 0;
  String json = "";
  String localtime = "";
  VERBOSE("CM : Retrieve time : ");
  http.begin("http://worldtimeapi.org/api/timezone/Asia/Taipei");
  httpCode = http.GET();

  if (httpCode > 0) {
    json = http.getString();
    deserializeJson(doc, json);
    // VERBOSELN(json);
     // 2019-08-01T09:08:59.665845+08:00
    localtime = doc["datetime"].as<char*>();
  } else {
    VERBOSELN("CM : Time server error");
  }
  http.end();   //Close connection
  VERBOSELN(localtime);

  // setTime(hr,min,sec,day,mnth,yr)`
  setTime(localtime.substring(11,13).toInt(),
    localtime.substring(14,16).toInt(),
    localtime.substring(17,19).toInt(),
    localtime.substring(8,10).toInt(),
    localtime.substring(5,7).toInt(),
    localtime.substring(0,4).toInt());
}
