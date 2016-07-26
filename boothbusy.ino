#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <FS.h>
#include "spiffs/spiffs.h"
#include "./DNSServer.h"

/* prototypes: */
void dots(void);

const byte DNSPORT = 53;
const byte HTTPCAPTIVE = 200;
const String appname = "Booth Busy";
const String footerText = "Booth Busy";
const char* roomName = "First Floor Booth 1";
const int WIFIMODE = WIFI_AP;
const char* ssid = ""; // to join a network, enter the ssid
const char* password = ""; // .. and password
const char* apName = "BOOTHBUSY"; // or set WIFIMODE == WIFI_AP and create one with captive portal
IPAddress apIP(192, 168, 42, 1); // network details (IP address) for access point mode, stolen from edison
const char* btnbg = "#ff6600";
const char* offbtnbg = "#fff";
typedef void callback_t(void);
int ticks = 0;

struct timer_config {
  int freq;
  callback_t* callback;
};

int n_timers = 1;
timer_config tmrs[] = {
  { .freq = 1000, .callback = dots }
};

struct gpio_config {
  byte pin;
  byte lastState;
  byte invert;
  char mode; // i, o, maybe p or i2s later
  char* label; // for buttons (mode=o)
  char* lowLabel; // for level display (mode=i)
  char* highLabel;
};

int n_gpios = 1;
gpio_config gpios[] = {
  { .pin = D2, .lastState = LOW, .invert=1, .mode = 'i', .label = "Motion Sensor", .lowLabel = "OCCUPIED", .highLabel = "Available" }
};

DNSServer dnsServer; 
ESP8266WebServer server(80);
File fsUploadFile;

void DBG(String) {
  
}
void dots(void) {
  Serial.print(".");
}
String style() {
  String css = String("body { background: #272c3b; height: 100%; width: 100%; padding: 0; ")
    + "font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Helvetica, Arial, sans-serif, \"Apple Color Emoji\", \"Segoe UI Emoji\", \"Segoe UI Symbol\";"
    + "font-size: 16px; line-height: 1.5;} " 
    + "h1 { color: white; display: block; font-size: 2em; font-weight: normal; padding-top: 32vh; text-align: center; text-transform: uppercase; } "
    + " .btn { float: left; display: block; line-height: 2em; margin-right: 1em; padding: 1em; } "
    + ".c0 { color: "+ String(offbtnbg) + "; } "
    + ".c1 { color: "+ String(btnbg) + "; } "
    + ".lvl { display: block; font-size: 5em; padding-top: 2vh; text-align: center; } "
    + "#footer { clear: both; padding-top: 1em; }"
    + "#menu { display: none; }"
    + "#page { background: url(bg.png) no-repeat; background-position: center center; background-size: contain; height: 100%; margin-top: 5vh; }";
  return css;
}
String javascript() {
  return String("<script>") 
  + "var $=function(x){return document.getElementById(x)};"
  + "$('page').onClick=function(){var e=document.documentElement;e.webkitRequestFullscreen()};"
  + "setInterval(function() { "
  + "var req=new XMLHttpRequest();"
  + "req.onload=function(){$('status').innerHTML=req.responseText};"
  + "req.open('GET', 'status', true);req.send();"
  + "}, 250); </script>";
}
String footer() {
  return "</div></div><div id=footer></div></body>"+javascript()+"</html>";
}
String menu() {
  return "<div id=menu><a href=/>App</a> | Manage: <a href=/wifi>Wifi</a> <a href=/files>Files</a></div>";
}
String header(String title) {
  return "<html><head><title>" + appname + "</title>" + 
    "<meta name=viewport content='width=device-width, user-scalable=no'/>" +
    "<style>" + style() + "</style></head><body><div id=page><h1>"+roomName+"</h1><div id=status>";
}
void send404() {
  server.send(HTTPCAPTIVE, "text/html", "Not Found");
}
// thanks to http://www.esp8266.com/viewtopic.php?f=29&t=6972#sthash.cafoIDRG.dpuf
bool tryFiles(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.html"; 
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf(".")); 
  // TODO this should be an array
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (!dataFile) { send404(); return false; }
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    dataFile.close();
    send404(); 
    return false;
  }
  dataFile.close();
  return true;
}
String status() {
  int i;
  String out, id;
  gpio_config g;
  for (i = 0; i < n_gpios; i++) {
    Serial.println(String(i) + "\n");
    g = gpios[i];
    id = String(g.pin);
    if (g.mode == 'o') {
      out += "<span class=btn id=g" + id + ">" + String(g.label) + "</span>";
    } else {
      byte r = gpios[i].lastState;
      out += "<span id=g" + id + " class='lvl c"+ (r?"0":"1") + "'>" + String(r ? g.highLabel : g.lowLabel) + "</span>";
    }
    Serial.println(out + "\n");
  }
  return out;
}
String index() {
  return header("Welcome") + status() + footer();
}
void handleFileUpload()
{
  if (server.uri() != "/files/upload") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("handleFileUpload Name: ");
    Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Serial.print("handleFileUpload Data: ");
    Serial.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: ");
    Serial.println(upload.totalSize);
  }
}

void handleListFiles()
{
  String UploadForm = "<form method='POST' action='/files/upload' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  String FileList = "File List:\n";

  Dir dir = SPIFFS.openDir("/");
  Serial.println("File List:");
  while (dir.next())
  {
    String FileName = dir.fileName();
    File f = dir.openFile("r");
    String FileSize = String(f.size());
    Serial.println(FileSize + " " + FileName);
    FileList += FileSize + " " + FileName + "<br/>";
  }
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", header("List of files") + FileList + UploadForm + footer());
}

void setup_gpio() {
  byte i;
  Serial.println("init GPIOs");
  pinMode(BUILTIN_LED, OUTPUT);
  for (i = 0; i < n_gpios; i++) {
    if (gpios[i].mode == 'i') {
      pinMode(gpios[i].pin, INPUT);
    }
    Serial.println(String(gpios[i].pin) + " " + String(gpios[i].mode) + ": " + String(gpios[i].label));
  }
}

void setup(void)
{
  SPIFFS.begin();
  Serial.begin(115200);
  Serial.println("8266kit");
  Serial.println("");
  setup_gpio();
  if(WIFIMODE == WIFI_AP_STA) {
   WiFi.mode(WIFI_AP_STA);
   WiFi.begin(ssid, password);
   if (WiFi.waitForConnectResult() == WL_CONNECTED)
   {
     Serial.print("Local IP: ");
     Serial.println(WiFi.localIP());
   }
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    char finalAp[30];
    snprintf(finalAp, sizeof(finalAp), "%s %s", apName, roomName); 
    WiFi.softAP(finalAp);
    dnsServer.start(DNSPORT, "*", apIP);
  }
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index());
  });
  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json", status());
  });
  server.on("/files", HTTP_GET, handleListFiles);
  server.onFileUpload(handleFileUpload);
  server.on("/files/upload", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "OK");
  });
  server.onNotFound([]() { tryFiles(server.uri()); });
  server.begin();
}

void gpio_changed(byte pin, byte state) {
  Serial.println(String(pin) + " " + state);
  digitalWrite(BUILTIN_LED, state);
}

void scan_gpios() {
  byte i;
  for (i = 0; i < n_gpios; i++) {
    if (gpios[i].mode == 'i') {
      byte r = digitalRead(gpios[i].pin);
      if(gpios[i].invert) r=!r;
      if (r != gpios[i].lastState) {
        gpio_changed(gpios[i].pin, r); gpios[i].lastState = r;
      }
    }
  }
}

void loop(void) {
  scan_gpios();
  if(WIFIMODE == WIFI_AP) dnsServer.processNextRequest();
  server.handleClient();
  ticks++;
  if (ticks % 10000 == 0) Serial.print(".");
  if (ticks % 100000 == 0) Serial.println("");
}

