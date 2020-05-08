/**
* Information Display Unit for Open Intellicence project
* https://github.com/norkator/Open-Intelligence
*/
// LIBRARIES
#include <ArduinoJson.h>
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <TaskScheduler.h>
#include <LiquidCrystal.h>
#include <dht.h>

// --------------------------------------------------------------------------------
// ## BASIC CONFIGURATION ##
const char* serverAddress = "192.168.2.35";
// --------------------------------------------------------------------------------

// ## PINS CONFIGURATION ##
// Wiznet ethernet module is using pins pins: 10, 4, 50, 51, 52, 53

const int lcd_RS_Pin    = 12; // RS
const int lcd_E_Pin     = 11; // Enable pin
const int lcd_D4_Pin    = 3; // D4
const int lcd_D5_Pin    = 5; // D5
const int lcd_D6_Pin    = 6; // D6
const int lcd_D7_Pin    = 7; // D7

// R/W pin ground, VSS pin ground, VCC pin 5V
LiquidCrystal lcd(lcd_RS_Pin, lcd_E_Pin, lcd_D4_Pin, lcd_D5_Pin, lcd_D6_Pin, lcd_D7_Pin);
const int buzzerPin     = 8;
const int relay1Pin     = 24; // "Tunnel" air blower
const int relay2Pin     = 25;
const int relay3Pin     = 26;
const int relay4Pin     = 27;
#define DHT22_1_PIN       28
#define DHT22_2_PIN       29

// --------------------------------------------------------------------------------

// ## ETHERNET CONFIG ##
EthernetClient client;
EthernetServer ethernetServer(80); // Port 80
byte mac[] = { 0xFA, 0xF4, 0x9A, 0xC7, 0xC6, 0xC2 }; // Mac address for ethernet nic
// IPAddress ip(192, 168, 1, 5); // we are using dhcp, no static ip needed
const char* server  = serverAddress;
const char* api     = "/arduino/display/data"; // API route
const int port      = 4300; // API listening port
const unsigned long HTTP_TIMEOUT = 10000; // Timeout for http call
const size_t MAX_CONTENT_SIZE = 124; // Must be incremented if http response comes out as null

// --------------------------------------------------------------------------------
// Variables

dht DHT;
String detectionsCount = "";
String personsCount = "";
String lastLp = "";
String relay1State = "Off";
String relay2State = "Off";
String relay3State = "Off";
String relay4State = "Off";
double dht22_1Temperature = 0.0;
int dht22_1Humidity = 0;
double dht22_2Temperature = 0.0;
int dht22_2Humidity = 0;

char linebuf[80];
int charcount=0;

// --------------------------------------------------------------------------------


// Task scheduler config
Scheduler runner;
Task t1(30 * 1000, TASK_FOREVER, &httpCallback); int httpSkip = 0; // Skip some
Task t2(40 * 1000, TASK_FOREVER, &dhtRead);

// ----------------------------------------------------------------------------------------------------------------------------------------------

// SETUP
void setup() {
  Serial.begin(9600);

  // init buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH); // Buzzer off

  // init relays
  pinMode(relay1Pin, OUTPUT);
  digitalWrite(relay1Pin, HIGH);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay2Pin, HIGH);
  pinMode(relay3Pin, OUTPUT);
  digitalWrite(relay3Pin, HIGH);
  pinMode(relay4Pin, OUTPUT);
  digitalWrite(relay4Pin, HIGH);

  // init lcd screen
  lcd.begin(16, 2); // Lcd columns and rows count

  // init ethernet
  if (!Ethernet.begin(mac)) { // DHCP
    Serial.println("Ethernet configure failed!");
    lcd.print("Ethernet configure failed!");
  } else {
    Serial.println("Ethernet init ok.");
    lcd.print(Ethernet.localIP());
    Serial.println(Ethernet.localIP());
  }
  
  // init task scheduler
  runner.init();
  // runner.addTask(t1);
  runner.addTask(t2);
  // t1.enable();
  t2.enable();

  delay(5 * 1000);
}


// Display dashboard page with on/off button for relay
// It also print Temperature in C and F
void dashboardPage(EthernetClient &client) {

  client.println("<!DOCTYPE HTML><html lang=\"en\"><head>");
  client.println("<meta charset=\"UTF-8\"><title>OI-Arduino Server</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<style>table {font-family: arial, sans-serif;border-collapse: collapse;display: inline-block;float: left;}  td, th {border: 1px solid #dddddd;text-align: left;padding: 5px;} button { width: 100%;}</style>");
  client.println("</head><body>");
  client.println("<h3><a href=\"/\">Refresh</a></h3>");

  // Blower control table | RELAY 1
  client.println("<table><tr><th>Blower relay</th></tr><tr><td>");
  if(relay1State == "Off"){
    client.println("<div style=\"background-color: red; width: 100%; height: 15px;\"></div>");
  } 
  else if(relay1State == "On"){
    client.println("<div style=\"background-color: green; width: 100%; height: 15px;\"></div>");                                                                    
  }
  client.println("</td></tr><tr><td>");
  if(relay1State == "Off"){
    client.println("<a href=\"/relay1on\"><button>ON</button></a>");
  } 
  else if(relay1State == "On"){
    client.println("<a href=\"/relay1off\"><button>OFF</button></a>");                                                                    
  }
  client.println("</td></tr></table>");
  
  
  // RELAY 2
  client.println("<table><tr><th>Relay 2</th></tr><tr><td>");
  if(relay2State == "Off"){
    client.println("<div style=\"background-color: red; width: 100%; height: 15px;\"></div>");
  } 
  else if(relay2State == "On"){
    client.println("<div style=\"background-color: green; width: 100%; height: 15px;\"></div>");                                                                    
  }
  client.println("</td></tr><tr><td>");
  if(relay2State == "Off"){
    client.println("<a href=\"/relay2on\"><button>ON</button></a>");
  } 
  else if(relay2State == "On"){
    client.println("<a href=\"/relay2off\"><button>OFF</button></a>");                                                                    
  }
  client.println("</td></tr></table>");


  // RELAY 3
  client.println("<table><tr><th>Relay 3</th></tr><tr><td>");
  if(relay3State == "Off"){
    client.println("<div style=\"background-color: red; width: 100%; height: 15px;\"></div>");
  } 
  else if(relay3State == "On"){
    client.println("<div style=\"background-color: green; width: 100%; height: 15px;\"></div>");                                                                    
  }
  client.println("</td></tr><tr><td>");
  if(relay3State == "Off"){
    client.println("<a href=\"/relay3on\"><button>ON</button></a>");
  } 
  else if(relay3State == "On"){
    client.println("<a href=\"/relay3off\"><button>OFF</button></a>");                                                                    
  }
  client.println("</td></tr></table>");


  // RELAY 4
  client.println("<table><tr><th>Relay 4</th></tr><tr><td>");
  if(relay4State == "Off"){
    client.println("<div style=\"background-color: red; width: 100%; height: 15px;\"></div>");
  } 
  else if(relay4State == "On"){
    client.println("<div style=\"background-color: green; width: 100%; height: 15px;\"></div>");                                                                    
  }
  client.println("</td></tr><tr><td>");
  if(relay4State == "Off"){
    client.println("<a href=\"/relay4on\"><button>ON</button></a>");
  } 
  else if(relay4State == "On"){
    client.println("<a href=\"/relay4off\"><button>OFF</button></a>");                                                                    
  }
  client.println("</td></tr></table>");


  // DHT22 tables
  client.println("<table><tr><th>DHT22-1 (Ceiling)</th></tr><tr><td>" + (String)dht22_1Temperature  + "°C</td></tr><tr><td>" + (String)dht22_1Humidity + "rH</td></tr></table>");
  client.println("<table><tr><th>DHT22-2 (Floor)</th></tr><tr><td>" + (String)dht22_2Temperature  + "°C</td></tr><tr><td>" + (String)dht22_2Humidity + "rH</td></tr></table>");
   
  client.println("</body></html>"); 
}

void webPageListenLoop() {
    // listen for incoming clients
  EthernetClient client = ethernetServer.available();
  if (client) {
    Serial.println("new client");
    memset(linebuf,0,sizeof(linebuf));
    charcount=0;
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
       char c = client.read();
       //read char by char HTTP request
        linebuf[charcount]=c;
        if (charcount<sizeof(linebuf)-1) charcount++;
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          dashboardPage(client);
          break;
        }
        if (c == '\n') {
          if (strstr(linebuf,"GET /relay1off") > 0){
            digitalWrite(relay1Pin, HIGH);
            relay1State = "Off";
          }
          else if (strstr(linebuf,"GET /relay1on") > 0){
            digitalWrite(relay1Pin, LOW);
            relay1State = "On";
          }
          
          if (strstr(linebuf,"GET /relay2off") > 0){
            digitalWrite(relay2Pin, HIGH);
            relay2State = "Off";
          }
          else if (strstr(linebuf,"GET /relay2on") > 0){
            digitalWrite(relay2Pin, LOW);
            relay2State = "On";
          }
          
          if (strstr(linebuf,"GET /relay3off") > 0){
            digitalWrite(relay3Pin, HIGH);
            relay3State = "Off";
          }
          else if (strstr(linebuf,"GET /relay3on") > 0){
            digitalWrite(relay3Pin, LOW);
            relay3State = "On";
          }
 
          if (strstr(linebuf,"GET /relay4off") > 0){
            digitalWrite(relay4Pin, HIGH);
            relay4State = "Off";
          }
          else if (strstr(linebuf,"GET /relay4on") > 0){
            digitalWrite(relay4Pin, LOW);
            relay4State = "On";
          }
          
          // you're starting a new line
          currentLineIsBlank = true;
          memset(linebuf,0,sizeof(linebuf));
          charcount=0;          
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
}


// --------------------------------------------------------------------------------

// LOOP
void loop() {
  runner.execute(); // Runner is responsible for all tasks
  webPageListenLoop();
}

// --------------------------------------------------------------------------------

// Http post operation
void httpCallback() {
    if (httpSkip == 0) {
      Serial.print("Http callback");
      if (connect(server)) {
        Serial.print(server); Serial.print(":"); Serial.print(port); Serial.println(api);
        if (sendRequest(server, api) && skipResponseHeaders()) {
          char response[MAX_CONTENT_SIZE];
          readReponseContent(response, sizeof(response));
          parseResponseData(response); 
        }
        disconnect();
      }
    }
    httpSkip = httpSkip +1;
    if (httpSkip == 3) {
      httpSkip = 0; 
    };
}

// --------------------------------------------------------------------------------------------------------------------------
// ----------------------------------------- H T T P - A P I - C A L L - T A S K --------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------


// Open connection to the HTTP server
bool connect(const char* hostName) {
  Serial.println("");
  Serial.print("Connecting to: ");
  Serial.print(hostName); Serial.print(":"); Serial.print(port);
  Serial.println("");
  bool ok = client.connect(hostName, port);
  Serial.println(ok ? "Connected" : "Connection Failed!");
  return ok;
}

// Send the HTTP POST request to the server, same time GET needed response data
bool sendRequest(const char* host, const char* api) {
  
  // Construct json parameter object
  StaticJsonBuffer<1200> jsonBuffer; // This seems not to affect anywhere
  StaticJsonBuffer<500> jsonBuffer2; // Increment more based on Object count
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& data = root.createNestedArray("data");

  // ---------------------------------------------------------------------------------s
  
  String postData = "";
  root.printTo(postData);
  Serial.println(postData);

  // Http post
  Serial.print("POST ");
  Serial.println(api);
  client.print("POST ");
  client.print(api);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(server);
  client.println("Content-Type: application/json");
  client.println("User-Agent: Arduino/1.0");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println();
  client.print(postData);
  client.println();
  
  return true;
}

// --------------------------------------------------------------------------------

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";
  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);
  if (!ok) {
    Serial.println("No response or invalid response!");
  }
  return ok;
}

// --------------------------------------------------------------------------------

// Read the body of the response from the HTTP server
void readReponseContent(char* content, size_t maxSize) {
  size_t length = client.readBytes(content, maxSize);
  content[length] = 0;
}

// --------------------------------------------------------------------------------

// Parse response data
void parseResponseData(char* content) {
  StaticJsonBuffer<MAX_CONTENT_SIZE> jsonBuffer; // Increment MAX_CONTENT_SIZE constant variable if repsonse is null
  JsonObject& root = jsonBuffer.parseObject(content);
  JsonObject& output = root["output"]; // All output content is inside this object
  
  Serial.println("---------- HTTP RESPONSE ----------");
  String responseData = "";
  output.printTo(responseData);
  Serial.println(responseData);
  Serial.println("-----------------------------------");

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return;
  }

  String oldLp = lastLp;
  String oldPersonsCount = personsCount;
  detectionsCount = output["detections_count"].as<String>();
  personsCount = output["persons_count"].as<String>();
  lastLp = output["last_lp"].as<String>();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DetCnt: " + detectionsCount);
  lcd.setCursor(0, 1);
  lcd.print("LstLP: " + lastLp);

  if (oldLp != lastLp) {
    buzz(500);
  }

  if (oldPersonsCount != personsCount) {
    buzz(250);
  }

}

// --------------------------------------------------------------------------------

// Close the connection with the HTTP server
void disconnect() {
  Serial.println("Disconnect");
  client.stop();
}

// --------------------------------------------------------------------------------


void buzz(int millis) {
  digitalWrite(buzzerPin, LOW); // BUZZ ON
  delay(millis);
  digitalWrite(buzzerPin, HIGH); // BUZZ OFF
}


// --------------------------------------------------------------------------------------------------------------------------
// -------------------------------------A L L - S E N S O R - R E A D I N G - T A S K S -------------------------------------
// --------------------------------------------------------------------------------------------------------------------------

// Read DHT temperature and humidity
void dhtRead() {
  int chk = DHT.read22(DHT22_1_PIN);  
  dht22_1Temperature = DHT.temperature;
  dht22_1Humidity = (DHT.humidity * 2);
  Serial.print("Temperature 1 val : ");
  Serial.print(dht22_1Temperature);
  Serial.println("°C");
  Serial.print("Humidity 1 val : ");
  Serial.print(dht22_1Humidity);
  Serial.println("rH");

  chk = DHT.read22(DHT22_2_PIN);  
  dht22_2Temperature = DHT.temperature;
  dht22_2Humidity = (DHT.humidity * 2);
  Serial.print("Temperature 2 val : ");
  Serial.print(dht22_2Temperature);
  Serial.println("°C");
  Serial.print("Humidity 2 val : ");
  Serial.print(dht22_2Humidity);
  Serial.println("rH");
}

// --------------------------------------------------------------------------------------------------------------------------
// -----------------------------P L A C E - H O L D E R - F O R - C O N T R O L  - T A S K S --------------------------------
// --------------------------------------------------------------------------------------------------------------------------

/* Code here */

// --------------------------------------------------------------------------------
