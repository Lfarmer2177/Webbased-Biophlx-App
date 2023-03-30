#include "WiFi.h"
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "FS.h"
#include <DHT.h>

DHT my_sensor(23, DHT22);
float temperature, humidity;

/****** For working in Station Mode in Development **********/
//const char* ssid = "AndroidAP";
//const char* password =  "abeera21";

/****** For working in AP mode in Production Server **********/
const char *ssid = "ESP32-AP";
const char *password =  "LetMeInPlz";
  

/****** Initialization of server **********/
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


/****** Sending Humidity and Temperature data to Server **********/
void notifyTempHumidity(){
    const uint8_t size = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<size> json;
    float temp, humid;
    //json["temp"] = random(11);
    //json["humid"] = random(11);

    json["temp"] = my_sensor.readTemperature();
    json["humid"] = my_sensor.readHumidity(); 
    delay(3000);


    char dataSend[40];
    serializeJsonPretty(json, Serial);
    size_t len = serializeJson(json, dataSend);
    ws.textAll(dataSend, len);
  
}


/****** Handle Incoming Socket Messages. But not needed now **********/
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      //ledState = !ledState;
      //notifyClients();
    }
  }
}


/****** Initializtion of Websockets **********/
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


/****** Reading File from SPIFFS **********/
void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    Serial.write(file.read());
  }
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}


 
void setup(){
  Serial.begin(115200);
    my_sensor.begin();

  
  if(!SPIFFS.begin()){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
  }
  
  //WiFi.begin(ssid, password);

    WiFi.softAP(ssid, password);

  // Print our IP address
  Serial.println();

 /****** Uncomment when using in station mode **********/
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(1000);
//    Serial.println("Connecting to WiFi..");
//  }
  
  //Serial.println(WiFi.localIP());
  Serial.println("AP running");
  Serial.print("My IP address: ");
  Serial.println(WiFi.softAPIP());


  initWebSocket();

  /*********Loading HTML CSS and JS from react build************/
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
   request->send(SPIFFS, "/index.html", "text/html");
  });
 
  server.on("/assets/index.9d5a143e.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/assets/index.9d5a143e.js", "text/javascript");
  });

  server.on("/assets/index.6ab4e619.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/assets/index.6ab4e619.css", "text/css");
  });


/****** Handle body of HTTTP request **********/
    server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    Serial.println("Running");
    if (request->url() == "/upload") {
      //DynamicJsonBuffer jsonBuffer;
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, (const char*)data);
      auto error = deserializeJson(doc, (const char*)data);
      if (error) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
      else{
      JsonObject object = doc.as<JsonObject>();
      const char* email = object["emaillog"];
      const char* password = object["passwordlog"];

/************* READ FROM FILE ************/
      File file = SPIFFS.open("/sys_config.json", "r");
      //readFile(SPIFFS, "/sys_config.json");
      DynamicJsonDocument doclogin(1024);
      DeserializationError error = deserializeJson(doclogin, file);
     if (error){Serial.println(F("Failed to read file, using default configuration"));}
      
      String data_str1 = doclogin["regUserName"].as<String>();
      String data_str2 = doclogin["regPassword"].as<String>();


      if ( (data_str1.equals(email)) && (data_str2.equals(password)) ) {
        /************* SENDING LOGIN SUCCESS ************/
        Serial.println("Passwords match");
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument json(1024);
        json["status"] = "success";
        //json["ssid"] = WiFi.SSID();
        //json["ip"] = WiFi.localIP().toString();
        serializeJson(json, *response);
        request->send(response);
        
      }
      else {
        Serial.println("Passwords not match");
        /************* SENDING LOGIN REGRET ************/
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument json(1024);
        json["status"] = "regret";
        serializeJson(json, *response);
        request->send(response);
        
      }
      file.close();
    
      
      serializeJsonPretty(doc, Serial);
      Serial.println(email);
      Serial.println(password);
    
      }
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      DynamicJsonDocument json(1024);
      json["status"] = "ok";
      json["ssid"] = WiFi.SSID();
      json["ip"] = WiFi.localIP().toString();
      serializeJson(json, *response);
      request->send(response);
    }


/* Registring the Data */
    if (request->url() == "/register") {
      //DynamicJsonBuffer jsonBuffer;
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, (const char*)data);
      auto error = deserializeJson(doc, (const char*)data);
      if (error) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
        else{
      JsonObject object = doc.as<JsonObject>();
      const char* email = object["regUserName"];
      const char* password = object["regPassword"];
      
      File file = SPIFFS.open("/sys_config.json", "w+");
      if(!file){
        Serial.println("OOps file is not present");
      }
      serializeJson(doc, file);
      file.close();

      listDir(SPIFFS, "/", 0);

      Serial.println("Reading JSON from SPIFFS: ");
      readFile(SPIFFS, "/sys_config.json");
      
      //serializeJsonPretty(doc, Serial);
      //Serial.println(email);
      //Serial.println(password);
    }
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      DynamicJsonDocument json(1024);
      json["status"] = "register";
      json["ssid"] = WiFi.SSID();
      json["ip"] = WiFi.localIP().toString();
      serializeJson(json, *response);
      request->send(response);
    }
    
  });



/* add these default headers for cross origion request*/
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");   
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
server.begin();

}
  

  
void loop(){
   ws.cleanupClients();
  notifyTempHumidity();
  
}
