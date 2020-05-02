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

// --------------------------------------------------------------------------------

// ## ETHERNET CONFIG ##
EthernetClient client;
byte mac[] = { 0xFA, 0xF4, 0x9A, 0xC7, 0xC6, 0xC2 }; // Mac address for ethernet nic
// IPAddress ip(192, 168, 1, 5); // we are using dhcp, no static ip needed
const char* server  = serverAddress;
const char* api     = "/arduino/display/data"; // API route
const int port      = 4300; // API listening port
const unsigned long HTTP_TIMEOUT = 10000; // Timeout for http call
const size_t MAX_CONTENT_SIZE = 124; // Must be incremented if http response comes out as null

// --------------------------------------------------------------------------------
// Variables

String detectionCount = "";
String lastLp = "";

// --------------------------------------------------------------------------------


// Task scheduler config
Scheduler runner;
Task t1(30 * 1000, TASK_FOREVER, &httpCallback); int httpSkip = 0; // Skip some

// ----------------------------------------------------------------------------------------------------------------------------------------------

// SETUP
void setup() {
  Serial.begin(9600);

  // init buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH); // Buzzer off

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
  t1.enable();

  delay(5 * 1000);
}

// --------------------------------------------------------------------------------

// LOOP
void loop() {
  runner.execute(); // Runner is responsible for all tasks
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
  detectionCount = output["detection_count"].as<String>();
  lastLp = output["last_lp"].as<String>();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DetCnt: " + detectionCount);
  lcd.setCursor(0, 1);
  lcd.print("LstLP: " + lastLp);

  if (oldLp != lastLp) {
    digitalWrite(buzzerPin, LOW); // BUZZ ON
    delay(500);
    digitalWrite(buzzerPin, HIGH); // BUZZ OFF
  }

}

// --------------------------------------------------------------------------------

// Close the connection with the HTTP server
void disconnect() {
  Serial.println("Disconnect");
  client.stop();
}

// --------------------------------------------------------------------------------



// --------------------------------------------------------------------------------------------------------------------------
// -----------------------------P L A C E - H O L D E R - F O R - C O N T R O L  - T A S K S --------------------------------
// --------------------------------------------------------------------------------------------------------------------------





// --------------------------------------------------------------------------------
