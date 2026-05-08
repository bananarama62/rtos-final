#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <ctime>

// Replace with your network credentials
const char* ssid = "";
const char* password = "";

// Stores which temperature profile currently using.
enum temperature_profile {
  COLD=0,
  MIDDLE=1,
  HOT=2,
  FORCE_ON=3,
  FORCE_OFF=4
};

// Stores information about a temperature profile. High value for turning on fan,
// low value for turning off fan
struct temperatures {
  int low;
  int high;
};

// The actual temperature profiles
const struct temperatures temperature_profiles[] = { 
  { 50, 60 }, // COLD
  { 60, 70 }, // MIDDLE
  { 75, 80 }  // HOT
};

const String profile_names[] { "COLD", "MIDDLE", "HOT" };

enum temperature_profile current_profile = MIDDLE; // Just set to middle, since user will change on browser

// Stores the motor's status (on/off)
enum item_status {
  OFF,
  ON
};

enum item_status motor_status = OFF; // Motor starts off

// I2C addresses for the temperature sensor
const byte SENSOR_ADDRESS = 0x40;
const byte TEMP_ADDRESS = 0x00;
const byte HUMIDITY_ADDRESS = 0x01;

double TEMPERATURE = 0.0; // Current temperature

// Semaphores used for adding mutual exclusion
SemaphoreHandle_t xTempSemaphore; // Setting/reading the temperature
SemaphoreHandle_t xMotorSemaphore; // Setting/reading whether motor is on or off


// Pins for the various motor wires
int motorPin1 = 14; // Blue
int motorPin2 = 4; // Pink
int motorPin3 = 5; // Yellow
int motorPin4 = 18; // Orange

// Motor rotation parameters
const int speed_low = 4800;
const int speed_mid = 2400;
const int speed_high = 1200;
int motorSpeed = speed_high; //variable to set stepper speed
int lookup[8] = {0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001, 0b1001};

// Writes the values for rotating the stepper motor
void setOutput(int out) {
  digitalWrite(motorPin1, bitRead(lookup[out], 0));
  digitalWrite(motorPin2, bitRead(lookup[out], 1));
  digitalWrite(motorPin3, bitRead(lookup[out], 2));
  digitalWrite(motorPin4, bitRead(lookup[out], 3));
}

// Create a web server object
WebServer server(80);

void RedirectToRoot(){
  String html = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"0; url=/\" /><body><p><a href=\"/\">Redirect should occur automatically. Click here if it doesn\'t</a></p></body></html>";
  server.send(200, "text/html", html);
}

// Sets profile to Cold
void HandleProfile1() {
  current_profile = COLD;
  RedirectToRoot();
}

// Sets profile to Middle
void HandleProfile2() {
  current_profile = MIDDLE;
  RedirectToRoot();
}

// Sets profile to Hot
void HandleProfile3() {
  current_profile = HOT;
  RedirectToRoot();
}

// Sets profile to Force on
void HandleForceOn() {
  current_profile = FORCE_ON;
  RedirectToRoot();
}

// Sets profile to Force off
void HandleForceOff() {
  current_profile = FORCE_OFF;
  RedirectToRoot();
}

void ButtonHTML(String* output, enum temperature_profile profile){

}

String ProfileInfo(enum temperature_profile profile){
  return "<p>Profile: " + profile_names[profile] + ". On at " + temperature_profiles[profile].high + ". Off at " + temperature_profiles[profile].low + "</p>";
}

// Function to handle the root URL and show the current states
void HandleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";
  html += "<p><a href=\"/\">root</a></p>";

  // Fetch current temperature
  double temp = -1.0;
  while ((xSemaphoreTake(xTempSemaphore, portMAX_DELAY) != pdTRUE)){
    ;
  }
  temp = TEMPERATURE;
  xSemaphoreGive(xTempSemaphore);

  time_t timestamp = time(NULL); // Current time

  html = html + "<p>Current Temperature: " + String(temp,2) + " degrees Fahrenheit</p>";
  html = html + "<p>Uptime: " + timestamp + " seconds</p>";

  html += ProfileInfo(COLD);
  html += ProfileInfo(MIDDLE);
  html += ProfileInfo(HOT);

  // Fan status
  html += "<p>Fan is currently: ";
  while ((xSemaphoreTake(xMotorSemaphore, portMAX_DELAY) != pdTRUE)){
    ;
  }
  if (motor_status == ON){
    html += "ON";
  } else {
    html += "OFF";
  }
  xSemaphoreGive(xMotorSemaphore);
  html += ".</p>";
  

  // Buttons for setting to Cold profile
  if (current_profile == COLD) {
    html += "<p><a href=\"/profile1\"><button class=\"button\">COLD</button></a></p>";
  } else {
    html += "<p><a href=\"/profile1\"><button class=\"button button2\">COLD</button></a></p>";
  }

  // Buttons for setting to Middle profile
  if (current_profile == MIDDLE) {
    html += "<p><a href=\"/profile2\"><button class=\"button\">MIDDLE</button></a></p>";
  } else {
    html += "<p><a href=\"/profile2\"><button class=\"button button2\">MIDDLE</button></a></p>";
  }

  // Buttons for setting to Hot profile
  if (current_profile == HOT) {
    html += "<p><a href=\"/profile3\"><button class=\"button\">HOT</button></a></p>";
  } else {
    html += "<p><a href=\"/profile3\"><button class=\"button button2\">HOT</button></a></p>";
  }

  // Buttons for setting to Force On profile
  if (current_profile == FORCE_ON) {
    html += "<p><a href=\"/force_on\"><button class=\"button\">FORCE ON</button></a></p>";
  } else {
    html += "<p><a href=\"/force_on\"><button class=\"button button2\">FORCE ON</button></a></p>";
  }

  // Buttons for setting to Force Off profile
  if (current_profile == FORCE_OFF) {
    html += "<p><a href=\"/force_off\"><button class=\"button\">FORCE OFF</button></a></p>";
  } else {
    html += "<p><a href=\"/force_off\"><button class=\"button button2\">FORCE OFF</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}


// Task for managing the web server
void TaskServer(void * parameters){
  // Handles connecting to the wifi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up the web server to handle different routes
  server.on("/", HandleRoot);
  server.on("/profile1", HandleProfile1);
  server.on("/profile2", HandleProfile2);
  server.on("/profile3", HandleProfile3);
  server.on("/force_on", HandleForceOn);
  server.on("/force_off", HandleForceOff);

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
  for (;;){
    // Handle incoming client requests
    server.handleClient();
    delay(25);
  }
}


// Used to indicate which I2C address to read from
// for the temperature sensor
void IndicateAddress(byte address){
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(address);
  Wire.endTransmission(SENSOR_ADDRESS);
}


// Periodicaly reads the temperature
void TaskTemperature(void * parameter){

  // Initialize I2C Wire
  Wire.begin();
  Wire.setClock(100000);
  
  // Configure settings on sensor
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(0x02);
  Wire.write(0x00); // Both temp and humidity at 14 bit resolution
  Wire.write(0x00);
  Wire.endTransmission(SENSOR_ADDRESS);
  delay(1000);

  // Continuously reads temperature
  for(;;){
    
    // Indicate to read temperature
    IndicateAddress(TEMP_ADDRESS);
    delay(100); // Gives the sensor time to take reading
    Wire.requestFrom(SENSOR_ADDRESS,2);
    
    uint16_t reading = 0; // Stores value readings
    reading = (Wire.read() << 8) | Wire.read(); // Temp requires 16 bits, so 2 bytes must be read and concatenated
    
    float temp = (float)reading / 65536.0 * 165.0 - 40.0; // Converts temp reading to Celsius
    temp = temp * 9.0 / 5.0 + 32; // Converts to fahrenheit
    
    while ((xSemaphoreTake(xTempSemaphore, portMAX_DELAY) != pdTRUE)){
      ;
    }
    TEMPERATURE = temp;
    xSemaphoreGive(xTempSemaphore);
    delay(1000); // Reads only every second
  }

}


// Task that sets motor state based on temperature
void TaskTempWatchdog(void * parameter){
  double temp = 0.0;
  for (;;){
    while ((xSemaphoreTake(xTempSemaphore, portMAX_DELAY) != pdTRUE)){
      ;
    }
    temp = TEMPERATURE;
    xSemaphoreGive(xTempSemaphore);
    Serial.println(temp);
    while ((xSemaphoreTake(xMotorSemaphore, portMAX_DELAY) != pdTRUE)){
      ;
    }
    if (current_profile == FORCE_OFF){
      motor_status = OFF;
    } else if (current_profile == FORCE_ON) {
      motor_status = ON;
    } else {
      // Temperature based logic
      if (motor_status == OFF){
        if (TEMPERATURE > temperature_profiles[current_profile].high){
          motor_status = ON;
        }
      } else {
        if (TEMPERATURE < temperature_profiles[current_profile].low){
          motor_status = OFF;
        }
      }
    }
    xSemaphoreGive(xMotorSemaphore);
    delay(1000);
  }
}


// Task for rotating the motor
void TaskMotor(void * parameter){
  // Declare the motor pins as outputs
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(motorPin3, OUTPUT);
  pinMode(motorPin4, OUTPUT);

  enum item_status spin = OFF;
  for (;;) {
    // Checks whether the motor needs to start spinning
    while ((xSemaphoreTake(xMotorSemaphore, portMAX_DELAY) != pdTRUE)){
      ;
    }
    spin = motor_status;
    
    xSemaphoreGive(xMotorSemaphore);
    if (spin == ON){
      // Spins the motor clockwise (counter clockwise when viewed head-on)
      for(int i = 7; i >= 0; i--) {
        setOutput(i);
        delayMicroseconds(motorSpeed);
      }
    }
    delay(25);
  }
}


void setup() {
  Serial.begin(115200);

  xTempSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xTempSemaphore);

  xMotorSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xMotorSemaphore);

  BaseType_t xReturned;
  
  TaskHandle_t xServerHandle = NULL;
  TaskHandle_t xTempHandle = NULL;
  TaskHandle_t xWatchdogHandle = NULL;
  TaskHandle_t xMotorHandle = NULL;

  xReturned = xTaskCreate(
    TaskServer,     // Function that implements the task. 
    "Server",       // Text name for the task. 
    5000,           // Stack size in words, not bytes. 
    NULL,           // Parameter passed into the task. 
    1,              // Priority at which the task is created. 
    &xServerHandle  // Used to pass out the created task's handle.
  );   

  xReturned = xTaskCreate(
    TaskTemperature, // Function that implements the task. 
    "Temperature",   // Text name for the task. 
    2000,            // Stack size in words, not bytes. 
    NULL,            // Parameter passed into the task. 
    1,               // Priority at which the task is created. 
    &xTempHandle     // Used to pass out the created task's handle.
  );   

  xReturned = xTaskCreate(
    TaskTempWatchdog, // Function that implements the task. 
    "Watchdog",       // Text name for the task. 
    2000,             // Stack size in words, not bytes. 
    NULL,             // Parameter passed into the task. 
    1,                // Priority at which the task is created. 
    &xWatchdogHandle  // Used to pass out the created task's handle.
  );   

  xReturned = xTaskCreate(
    TaskMotor,     // Function that implements the task. 
    "Motor",       // Text name for the task. 
    2000,          // Stack size in words, not bytes. 
    NULL,          // Parameter passed into the task. 
    1,             // Priority at which the task is created. 
    &xMotorHandle  // Used to pass out the created task's handle.
  );   

}

void loop() {

}
