#include <DHT.h>
#include <FS.h>
#include <WiFiClient.h> 
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

//Define MCU Digital Pins
#define D0 16
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0
#define D4 2 // Same as "LED_BUILTIN", but inverted logic
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO 
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS)
#define D9 3 // RX0 (Serial console)
#define D10 1 // TX0 (Serial console)

//DHT22
#define DHTTYPE DHT22
DHT dht(D4, DHTTYPE);

//PMS5003
#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
unsigned char buf[LENG];
int PM01Value = 0;          
int PM25Value = 0;
int PM10Value = 0;

/* Set these to your desired credentials. */
const char* defaultSSID = "MSN-Box_Wifi";
const char* defaultPWD = "12345678";

//StaticJsonBuffer<100> jsonConfigBuffer;
//JsonObject& wifiConfig = jsonConfigBuffer.createObject();


const char* host = "118.70.72.15";
const int httpPort = 2222;

const int delayTime = 5; //seconds
ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  SPIFFS.begin();
  
  //Set LED pins as ouput
  pinMode(D0, OUTPUT); //RED
  pinMode(D1, OUTPUT); //YELLOW 
  pinMode(D2, OUTPUT); //GREEN

  turnON(D0);
  
  start();
}

void loop() {
  if(WiFi.status() == WL_CONNECTION_LOST){
    turnOFF(D1);
    startAP();
  } else {
    if(digitalRead(D1) == HIGH){
      if(postData()){
        turnON(D2);
        delay(delayTime*1000);
      } else 
        turnOFF(D2);
    } else 
      turnOFF(D2);
  }
  server.handleClient();
}

void handleRoot() {
  server.send(200, "text/html", "ON");
}

void handleScan() {
  server.send(200, "application/json", scanWifi());
}

void handleData() {
  server.send(200, "application/json", readData());
}

void handleConfig() {
  StaticJsonBuffer<100> jsonConfigBuffer;
  JsonObject& wifiConfig = jsonConfigBuffer.createObject();
  wifiConfig["ssid"] = server.arg("ssid");
  wifiConfig["password"] = server.arg("pwd");
  writeConfig(wifiConfig);
  server.send(200, "text/html", "1");
  start();
}

void start(){
  turnOFF(D2);
  connectWifi(readConfig());
  if(WiFi.status() == WL_CONNECTED){
    //Serial.println(WiFi.localIP());
    turnON(D1);
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/scan", handleScan);
    server.on("/config", handleConfig);
    server.begin();
  } else {
    startAP();
  }
}
void writeConfig(JsonObject& wifiConfig){
    // open file for writing
  File f = SPIFFS.open("/config.json", "w");
  if (!f) {
      //Serial.println("file open failed");
  }
  String wifiConfigString;
  wifiConfig.printTo(wifiConfigString);
  // write ssid and pwd to file
  f.println(wifiConfigString);
  f.close();
  
  //Serial.println("Wroted");
}

JsonObject& readConfig(){
  StaticJsonBuffer<100> jsonConfigBuffer;
  String stringConfig;
  // open file for reading
  File f = SPIFFS.open("/config.json", "r");
  if (!f) {
      //Serial.println("file open failed");
  }
  // read wifi SSID config
  while(f.available()){
    stringConfig += f.readStringUntil('\n');
  }
  char charsConfig[stringConfig.length()];
  stringConfig.toCharArray(charsConfig, stringConfig.length());
  return jsonConfigBuffer.parseObject(charsConfig);
}

void connectWifi(JsonObject& wifiConfig){
  WiFi.mode(WIFI_STA); //WiFi.mode(m): set mode to WIFI_AP, WIFI_STA, or WIFI_AP_STA
  delay(1000);
  const char* ssid = wifiConfig["ssid"];
  const char* password = wifiConfig["password"];
  WiFi.begin(ssid, password);
  int i;
  for(i=0; i < 10 ; i++){
    turnON(D1);
    delay(1000);
    turnOFF(D1);
    delay(1000);
    if(WiFi.status() == WL_CONNECTED)
      break;
  }
}

void startAP(){
  WiFi.disconnect();
  WiFi.mode(WIFI_AP); //WiFi.mode(m): set mode to WIFI_AP, WIFI_STA, or WIFI_AP_STA
  delay(1000);
  WiFi.softAP(defaultSSID, defaultPWD);
  //IPAddress myIP = WiFi.softAPIP();
  //Serial.print("AP IP address: ");
  //Serial.println(myIP);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/scan", handleScan);
  server.on("/config", handleConfig);
  server.begin();
}

String scanWifi(){
  StaticJsonBuffer<2000> jsonScanBuffer;
  int n = WiFi.scanNetworks();
  JsonObject& list = jsonScanBuffer.createObject();
  JsonArray& listSSID = list.createNestedArray("ssid");
  if (n > 0)
  {
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      JsonObject& ssid = listSSID.createNestedObject();
      ssid["id"] = i;
      ssid["ssid"] = WiFi.SSID(i);
      ssid["rssi"] = WiFi.RSSI(i);
      ssid["isOpen"] = (WiFi.encryptionType(i) == ENC_TYPE_NONE)? true : false;
    }
  }
  String result;
  list.printTo(result);
  return result;
}

void turnON(int led){
  digitalWrite(led, HIGH);
}

void turnOFF(int led){
  digitalWrite(led, LOW);
}
/*
//Read data from 2 sensor
String readData(){
  StaticJsonBuffer<200> jsonResultBuffer;
  JsonObject& result = jsonResultBuffer.createObject();
  
  if(Serial.find(0x42)){    //start to read when detect 0x42
    Serial.readBytes(buf,LENG);
 
    if(buf[0] == 0x4d){
      if(checkValue(buf,LENG)){
        result["PMSisError"] = false;
        PM01Value = transmitPM01(buf); //count PM1.0 value of the air detector module
        PM25Value = transmitPM25(buf);//count PM2.5 value of the air detector module
        PM10Value = transmitPM10(buf); //count PM10 value of the air detector module 
      }           
    } else 
        result["PMSisError"] = false;
  } else 
        result["PMSisError"] = false;
 
  static unsigned long OledTimer=millis();  
    if (millis() - OledTimer >=1000) 
    {
      OledTimer=millis();
      result["pm1"] = PM01Value;     
      result["pm25"] = PM25Value;   
      result["pm10"] = PM10Value; 
    }
  float h = dht.readHumidity();
  // Read temperature as Celsius
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    result["DHTisError"] = true;
  } else {
    result["DHTisError"] = false;
    result["hud"] = h;
    result["temp"] = t;
  }

  String resultString;
  result.printTo(resultString);
  return resultString;
}
*/

//Read data from 2 sensor
String readData(){
  String result;  
  float h = dht.readHumidity();
  // Read temperature as Celsius
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    return "";
  } else {
    result += "h=" + String(h) + "&t=" + String(t);
  }
  
  if(Serial.find(0x42)){    //start to read when detect 0x42
    Serial.readBytes(buf,LENG);
 
    if(buf[0] == 0x4d){
      if(checkValue(buf,LENG)){
        PM01Value = transmitPM01(buf); //count PM1.0 value of the air detector module
        PM25Value = transmitPM25(buf);//count PM2.5 value of the air detector module
        PM10Value = transmitPM10(buf); //count PM10 value of the air detector module 
      }           
    }
  }
 
  static unsigned long OledTimer=millis();  
    if (millis() - OledTimer >=1000) 
    {
      OledTimer=millis();
      result += "&pm1=" + String(PM01Value);     
      result += "&pm25=" + String(PM25Value);   
      result += "&pm10=" + String(PM10Value); 
    }

  return result;
}
boolean postData(){
  String url = "/data.php?" + readData();
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    //Serial.println("connection failed");
    return false;
  }
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      //Serial.println(">>> Client Timeout !");
      client.stop();
      return false;
    }
  }

  return true;
}

//Check pms5003 module
char checkValue(unsigned char *thebuf, char leng)
{  
  char receiveflag=0;
  int receiveSum=0;
 
  for(int i=0; i<(leng-2); i++){
  receiveSum=receiveSum+thebuf[i];
  }
  receiveSum=receiveSum + 0x42;
  
  if(receiveSum == ((thebuf[leng-2]<<8)+thebuf[leng-1]))  //check the serial data 
  {
    receiveSum = 0;
    receiveflag = 1;
  }
  return receiveflag;
}

//transmit PM 1.0
int transmitPM01(unsigned char *thebuf)
{
  int PM01Val;
  PM01Val = ((thebuf[3]<<8) + thebuf[4]); //count PM1.0 value of the air detector module
  return PM01Val;
}
 
//transmit PM 2.5
int transmitPM25(unsigned char *thebuf)
{
  int PM25Val;
  PM25Val = ((thebuf[5]<<8) + thebuf[6]); //count PM2.5 value of the air detector module
  return PM25Val;
}
 
//transmit PM 10
int transmitPM10(unsigned char *thebuf)
{
  int PM10Val;
  PM10Val = ((thebuf[7]<<8) + thebuf[8]); //count PM10 value of the air detector module  
  return PM10Val;
}
