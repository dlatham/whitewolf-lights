#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266mDNS.h>          //Support .local URLs
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <FS.h>                   //Handles saving files to SPIFFS
#include <ArduinoJson.h>          //For configuration files which are sent / stored in JSON
#include <ArduinoOTA.h>           //Handle updates OTA
//#include <WebOTA.h> --- In the future I should switch to this when the server is ready

#define PIN 4 // Which pin on the Arduino is connected to the NeoPixels?
#define NUMPIXELS 16 // How many NeoPixels are attached to the Arduino?
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
uint32_t warmWhite = pixels.Color(255, 0, 0); // Default light color (warm white)
uint32_t black = pixels.Color(0, 0, 0);
ESP8266WebServer server(80);
const size_t capacity = JSON_ARRAY_SIZE(3) + 3*JSON_ARRAY_SIZE(16) + 49*JSON_OBJECT_SIZE(3) + 860;
DynamicJsonBuffer jsonBuffer(capacity);
String header; //variable to store the HTTP request
float ver = 0.1; //Update for response to the server

// SERVER HANDLER FUNCTIONS------------------------------------------>
void handleStatus(){
  Serial.println(F("SERVER: Status request"));
  server.send(200, "application/json", "{\"status\":\"ok\",\"version\":\"" + String(ver) + "\"}");
}

void handleColor(){
  Serial.println(F("SERVER: Color change request."));
  uint8_t r=0, g=0, b=0;
  r=atoi(server.arg("r").c_str());
  g=atoi(server.arg("g").c_str());
  b=atoi(server.arg("b").c_str());
  Serial.println("Setting pixels to color [" + String(r) + "," + String(g) + "," + String(b) + "]");
  for(uint8_t i=0; i<NUMPIXELS; i++){
    pixels.setPixelColor(i, r, g, b);
  }
  pixels.show();
  server.send(200, "text/plain", "Color change request successful.");
}

void handlePowerConfig(){
  JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));
  File file = SPIFFS.open("/power_config.txt","w");
  if(!file){
    server.send(500, "text/plain", "Unable to open config file for saving.");
  } else {
    if (root.printTo(file) == 0) {
      server.send(500, "text/plain", "Unable to save to config file.");
    } else {
      server.sendHeader("Access-Control-Allow-Origin","*");
      server.send(200, "text/plain", "Ok: New config stored.");
    }
    file.close();
  }
}

void handleSwitchConfig(){
  JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));
  File file = SPIFFS.open("/switch_config.txt","w");
  if(!file){
    server.send(500, "text/plain", "Unable to open config file for saving.");
  } else {
    if (root.printTo(file) == 0) {
      server.send(500, "text/plain", "Unable to save to config file.");
    } else {
      server.sendHeader("Access-Control-Allow-Origin","*");
      server.send(200, "text/plain", "Ok: New config stored.");
    }
    file.close();
  }
}

void handlePowerConfigGet(){
  File file = SPIFFS.open("/power_config.txt", "r");
  if(!file){
    server.send(500, "text/plain", "Unable to open config file.");
  } else {
    server.streamFile(file, "application/json");
    file.close();
  }
}

void handleSwitchConfigGet(){
  File file = SPIFFS.open("/switch_config.txt", "r");
  if(!file){
    server.send(500, "text/plain", "Unable to open config file.");
  } else {
    server.streamFile(file, "application/json");
    file.close();
  }
}



void handleNotFound(){
  Serial.println(F("SERVER: Not found"));
  server.send(404, "text/plain", "Not found.");
}

// PROGRAM SETUP-------------------------------------------------------->
void setup() {
  Serial.begin(115200);
  Serial.println("White Wolf Lights v0.1");

  //Setup file storage
  SPIFFS.begin();
  
  pixels.begin(); //Start the pixels up first before the web stuff
  powerOnColor();

  //Wifi setup
  WiFi.hostname("whitewolf-lights");
  WiFiManager wifiManager; // Start the wifimanager
  //first parameter is name of access point, second is the password
  wifiManager.autoConnect("whitewolf-lights", "whitewolf");

  //mDNS Setup
  if (!MDNS.begin("wifilights")) {             // Start the mDNS responder for wifilights.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  //Server setup
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/color", HTTP_GET, handleColor);
  server.on("/config/power", HTTP_POST, handlePowerConfig);
  server.on("/config/power", HTTP_GET, handlePowerConfigGet);
  server.on("/config/switch", HTTP_POST, handleSwitchConfig);
  server.on("/config/switch", HTTP_GET, handleSwitchConfigGet);
  server.onNotFound(handleNotFound);
  server.begin();

  //OTA Setup
  // All of this code below should get replaced when I switch to the webOTA framework in production...
  // To use a specific port and path uncomment this line
  // Defaults are 8080 and "/webota"
  // webota.init(8888, "/update");
  // BUT FOR NOW>>>>
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
  ArduinoOTA.begin();
  Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());

}

void loop() {
  /* the restful API will function like the following:
   * -------------------------------------------------
   * /color?r=128&g=128&b=128  Change all LEDs to the rgb value
   * /on?o=255  Turn all LEDs on to the specifid opacity (optional)
   * /off . Turn all LEDs off
   */
  server.handleClient();
  ArduinoOTA.handle();
  //webota.handle();
   
}

void powerOnColor(){
  // Handle setting the initial color of the lights on startup
  Serial.println("Starting initial file read...");
  File file = SPIFFS.open("/power_config.txt", "r");
  if(!file){
    //No configuration file so just set them all off
    Serial.println("No power_configuration file found.");
    pixels.clear();
  } else {
    // Parse the colors from the config file
    size_t size = file.size();
    std::unique_ptr<char[]> buf (new char[size]);
    file.readBytes(buf.get(), size);
    JsonObject& root = jsonBuffer.parseObject(buf.get());
    for(uint8_t i=0; i<NUMPIXELS; i++){
      int r = atoi(root["data"][i]["r"]);
      int g = atoi(root["data"][i]["g"]);
      int b = atoi(root["data"][i]["b"]);
      //color = pixels.Color(root["data"][i]["r"], root["data"][i]["g"], root["data"][i]["b"]);
      Serial.println("Setting light [" + String(i) + "] to color [" + r + "," + g + "," + b + "]");
      pixels.setPixelColor(i, r, g, b);
    }
    pixels.show();
  }
}
