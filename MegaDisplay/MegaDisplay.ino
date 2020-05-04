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
  runner.addTask(t1);
  runner.addTask(t2);
  t1.enable();
  t2.enable();

  delay(5 * 1000);
}


// Display dashboard page with on/off button for relay
// It also print Temperature in C and F
void dashboardPage(EthernetClient &client) {
  client.println("<!DOCTYPE HTML><html><head>");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body>");                                                             
  client.println("<h3>Arduino Web Server - <a href=\"/\">Refresh</a></h3>");
  // Generates buttons to control the relay
  client.println("<h4>Relay 1 - State: " + relay1State + "</h4>");
  // If relay is off, it shows the button to turn the output on          
  if(relay1State == "Off"){
    client.println("<a href=\"/relay1on\"><button>ON</button></a>");
  }
  // If relay is on, it shows the button to turn the output off         
  else if(relay1State == "On"){
    client.println("<a href=\"/relay1off\"><button>OFF</button></a>");                                                                    
  }
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
