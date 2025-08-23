#include <AS5600.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define PAD0 0
#define PAD1 1
#define PAD2 2
#define PAD3 3

//wifi
const char* ssid="iot";
const char* password="jacolr@telkom!9";
const String configFileName="config.json";

//TFT en fonts
TFT_eSPI tft = TFT_eSPI();

//webserver
WebServer server(80);

//String page="<html><head><title>Configuration File Uploader</title><style><body backgroud-color: #305a36; font-family: Arial; Color black></style></head><body><div style=\"text-align: center;backgound-color: rgb(189,253,253);padding: 20px;margin: 20px;border-radius: 25px;\"><form method=\"post\" action=\"upload\" enctype=\"multipart/form-data\"><input style=\"text-align: center;\" type= \"file\" name=\"upload\"><br></br><input type=\"submit\" value=\"Upload\"></form></div><form method=\"get\" action=\"download\" enctype=\"multipart/form-data\"><input style=\"text-align: center;\" type= \"file\" name=\"file\"><br></br><input type=\"submit\" value=\"Download\"></form></body></html>";

String page = "<!DOCTYPE html><html><head><title>Configurator</title></head><style>div {background-color: lightgreen;padding: 20px;  border-radius: 20px;}input {border-radius: 10px;color: white;background-color: darkgreen;}</style><body style=\"background-color:lightgrey;font-family:Arial\"><h1>Configurator</h1><div><form action=\"/download?file=config.txt\"><input type=\"submit\" value=\"Download Configuraion\"></form></div><br></br><div><form action=\"/upload\" enctype=\"multipart/form-data\" method=\"post\"><input type=\"file\" name=\"upload\"><br></br><input type=\"submit\" value=\"Upload Configuration\"></form></div></body></html>";
String pageFileUploadError = "<body><style>div {font-family:arial;background-color: darkgreen;color: white;border-radius:20px;padding: 25px;}</style><div><p>File upload failed, use the correct config.json file.</p><a style=\"color:white\" href=\"/\">Click here to try again...</a></div></body>";

//general
bool NewUpload = false;
const char* arrayPadList[4];
const char* arrayKeys[4][10]; //4[PADS] dimentions with 10 key combinations in each

//Rotary Encoder
AS5600 rotEnc;
#define rotSDA 25
#define rotSCL 33

int rotReadTime_ms = 250; 

String createResponse(){
  return page;
}

void rootResponse(){
  server.send(200, "text/html", createResponse());
}

void notFound(){

String response = "File not Found\n";
response += "URI:" + server.uri() +"\nMethod:" + (server.method()== HTTP_GET? "GET" : "POST") + "\n";
server.send(404, "text/plain", response);

}
void downloadFunction(){

  File download = SPIFFS.open("/" + configFileName ,"r");
  if(download){

  server.sendHeader("Content-Disposition:", "attachement; filename=" + configFileName);
  server.sendHeader("Content-Type", "application/oxtet-stream");
  server.streamFile(download, "text/text", 200);
  
  download.close();
}
else {
  server.send(400, "text/plain","No file to download.");
}

}
void uploadFunction(){
  Serial.println("Upload Started");
  
  HTTPUpload upload = server.upload();

  if(upload.filename != configFileName){
    server.send(400,"text/html",pageFileUploadError);
    return;
  }

  File file;

  SPIFFS.remove("/" + configFileName);

  file = SPIFFS.open("/" + configFileName, FILE_WRITE );

  if(!file) {
    Serial.println("Creating File Failed!!!");
    return;
  }

  file.write(upload.buf, upload.currentSize);

  if(upload.status == UPLOAD_FILE_END){

  Serial.println("\n\nUpload complete");

  file.close();

  server.send( 200, "text/html", "<a href= \"/\">Upload successesfull...click to return.</a>");

  }

  if(!SPIFFS.exists("/" + configFileName)){
    Serial.println("File not available.");
  }

  File fread = SPIFFS.open("/" + configFileName);

  int size = fread.size();

  String message = "File Size: ";
  message += size;
  message += "\n";
  message += "Content:\n";
  String content;
  
  while (fread.available()){
    content += fread.readString();
  }
  
  fread.close();

  Serial.print("Read Content:\n");
  Serial.println(content);
  
  message += content;

  server.send(200, "text/html", page);

  NewUpload = true;

}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(">");
  }

  Serial.println("\nConnected...");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  //MDNS.begin("ble_go");

 server.on("/", rootResponse);
 server.on("/upload", uploadFunction);
 server.on("/download",HTTP_GET, downloadFunction);
 server.onNotFound(notFound);
 
 server.begin();

 if (SPIFFS.begin(true)){
    Serial.println("FS ready!");
 }
 else {
   Serial.println("FS Failed!");
 }


//enable  rotary encoder;
Wire.begin(rotSDA,rotSCL);


//tft configuration
  tft.begin();
  tft.setRotation(1);
  tft.setTextFont(4);
  tft.setTextSize(1.2);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10,45);
  tft.println("ble Multi Key");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10,100);
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  delay(2000);
  
  //Load config file already in SPIFFS
  ConfigurationFileParser();
  
 // tft.fillScreen(TFT_BLACK);

  //UpdateHeaderText("General", TFT_DARKGREEN);
}

void loop() {
 
static uint8_t lastTime = 0;

//rotary encoder
if (millis() - lastTime >= rotReadTime_ms) {
    lastTime = millis();
    rotEnc.getCumulativePosition();    
}

  //config webserver
 server.handleClient();

  //handle new config
  if(NewUpload){

    NewUpload = false;
    ConfigurationFileParser();
    UpdateHeaderText(arrayPadList[0], TFT_BLACK );
  }

}

void ConfigurationFileParser(){

  Serial.println("Configuration File Parser");  


  //Read the Main Pad Info
  File file = SPIFFS.open("/" + configFileName, "r");

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, file);

  file.close();  

  if(error){
    Serial.println("Failed to read file!");
    NoFileTFTMessage();
    delay(5000);
    return;
  }

  JsonArray arrayJSON = doc["PadList"];

  Serial.print("PadList:");
  Serial.println(arrayJSON.size());

  int i = 0;

  for( JsonObject data : arrayJSON ){
  
      const char* value = data["Pad"];
      arrayPadList[i] = value;
      i++;
  }

i=0;
// Read the Key Assignments per Pad
arrayJSON = doc["Pad0"];

  Serial.print("Pad0:");
  Serial.println(arrayJSON.size());

  for( JsonObject data : arrayJSON ){
  
      const char* value = data["value"];
      arrayKeys[PAD0][i] = value;
      i++;
  }

  Serial.println(arrayKeys[PAD0][1]);

  i=0;
// Read the Key Assignments per Pad
arrayJSON = doc["Pad1"];

  Serial.print("Pad1:");
  Serial.println(arrayJSON.size());

  for( JsonObject data : arrayJSON ){
  
      const char* value = data["value"];
      arrayKeys[PAD1][i] = value;
      i++;
  }

  Serial.println(arrayKeys[PAD1][1]);

  i=0;
// Read the Key Assignments per Pad
arrayJSON = doc["Pad2"];

  Serial.print("Pad2:");
  Serial.println(arrayJSON.size());

  for( JsonObject data : arrayJSON ){
  
      const char* value = data["value"];
      arrayKeys[PAD2][i] = value;
      i++;
  }

  Serial.println(arrayKeys[PAD2][1]);

  i=0;
// Read the Key Assignments per Pad
arrayJSON = doc["Pad3"];

  Serial.print("Pad3:");
  Serial.println(arrayJSON.size());

  for( JsonObject data : arrayJSON ){
  
      const char* value = data["value"];
      arrayKeys[PAD3][i] = value;
      i++;
  }

  Serial.println(arrayKeys[PAD3][1]);
}
void NoFileTFTMessage(){

    tft.setTextFont(2);
    tft.fillScreen(TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setCursor(10,10);
    tft.println("No Configuration File\n\n");

    tft.setCursor(10,90);
    tft.println(WiFi.localIP());
    tft.setCursor(10,50);
    tft.println("To upload configuration use IP:");
}


void UpdateHeaderText(String Line, uint32_t themecolor){


  tft.fillRoundRect(5, 5, 230, 125, 10, TFT_DARKGREY);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_DARKGREY);
  tft.drawString(Line, 20,12, 4);

  tft.fillSmoothCircle( 213, 20, 3, themecolor, TFT_DARKGREY);
  tft.fillSmoothCircle( 213, 115, 3, themecolor, TFT_DARKGREY);
  tft.fillRect(210, 20 , 7,  95, themecolor);

}