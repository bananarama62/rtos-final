#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid = "";
const char* password = "";

enum temperature_profile {
  COLD=0,
  MIDDLE=1,
  HOT=2,
  FORCE_ON=3,
  FORCE_OFF=4
};

enum item_status {
  OFF,
  ON
};

enum item_status motor_status = OFF;

enum temperature_profile current_profile = MIDDLE;

struct temperatures {
  int low;
  int high;
};

const struct temperatures temperature_profiles[] = { 
  { 50, 60 }, // COLD
  { 60, 70 }, // MIDDLE
  { 70, 80 }  // HOT
};

const byte SENSOR_ADDRESS = 0x40;
const byte TEMP_ADDRESS = 0x00;
const byte HUMIDITY_ADDRESS = 0x01;

SemaphoreHandle_t xTempSemaphore;
SemaphoreHandle_t xMotorSemaphore;

double TEMPERATURE = 0.0;

// Pins for the various motor wires
int motorPin1 = 14; // Blue
int motorPin2 = 4; // Pink
int motorPin3 = 5; // Yellow
int motorPin4 = 18; // Orange

// Motor rotation parameters
const int speed_low = 4800;
const int speed_mid = 2400;
const int speed_high = 1200;
int motorSpeed = speed_low; //variable to set stepper speed
int count = 0; // count of steps made
int countsperrev = 512; // number of steps per full revolution
int lookup[8] = {0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001, 0b1001};

void setOutput(int out) {
  digitalWrite(motorPin1, bitRead(lookup[out], 0));
  digitalWrite(motorPin2, bitRead(lookup[out], 1));
  digitalWrite(motorPin3, bitRead(lookup[out], 2));
  digitalWrite(motorPin4, bitRead(lookup[out], 3));
}

// Create a web server object
WebServer server(80);

void HandleProfile1() {
  current_profile = COLD;
  HandleRoot();
}

void HandleProfile2() {
  current_profile = MIDDLE;
  HandleRoot();
}

void HandleProfile3() {
  current_profile = HOT;
  HandleRoot();
}

// Function to handle the root URL and show the current states
void HandleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";

  if (current_profile == COLD) {
    html += "<p><a href=\"/profile1\"><button class=\"button\">COLD</button></a></p>";
  } else {
    html += "<p><a href=\"/profile1\"><button class=\"button button2\">COLD</button></a></p>";
  }

  if (current_profile == MIDDLE) {
    html += "<p><a href=\"/profile2\"><button class=\"button\">MIDDLE</button></a></p>";
  } else {
    html += "<p><a href=\"/profile2\"><button class=\"button button2\">MIDDLE</button></a></p>";
  }

  if (current_profile == HOT) {
    html += "<p><a href=\"/profile3\"><button class=\"button\">HOT</button></a></p>";
  } else {
    html += "<p><a href=\"/profile3\"><button class=\"button button2\">HOT</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}


void TaskServer(void * parameters){
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

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
  for (;;){
    // Handle incoming client requests
    //Serial.println("Handle client");
    server.handleClient();
    delay(25);
  }
}

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
    // Pause between temperature and humidity readings
    xSemaphoreGive(xTempSemaphore);
    delay(1000);
  }

}

void TaskTempWatchdog(void * parameter){
  for (;;){
    while ((xSemaphoreTake(xTempSemaphore, portMAX_DELAY) != pdTRUE)){
      ;
    }
    Serial.println(TEMPERATURE);
    if (current_profile == FORCE_OFF){
      motor_status = OFF;
    } else if (current_profile == FORCE_ON) {
      motor_status = ON;
    } else {
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
    xSemaphoreGive(xTempSemaphore);
    delay(1000);
  }
}

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
      //Serial.println(spin);
      // Spins the motor clockwise (counter clockwise when viewed head-on)
      for(int i = 7; i >= 0; i--) {
        setOutput(i);
        delayMicroseconds(motorSpeed);
      }
    }
    delay(25);
  }
}

void IndicateAddress(byte address){
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(address);
  Wire.endTransmission(SENSOR_ADDRESS);
}

void setup() {
  Serial.begin(115200);

  
  

  xTempSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xTempSemaphore);

  xMotorSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(xMotorSemaphore);

  // Connect to Wi-Fi network
  

  BaseType_t xReturned;
  
  TaskHandle_t xServerHandle = NULL;
  TaskHandle_t xTempHandle = NULL;
  TaskHandle_t xWatchdogHandle = NULL;
  TaskHandle_t xMotorHandle = NULL;

  xReturned = xTaskCreate(
    TaskServer,    // Function that implements the task. 
    "Server",       // Text name for the task. 
    5000,           // Stack size in words, not bytes. 
    NULL,           // Parameter passed into the task. 
    1,              // Priority at which the task is created. 
    &xServerHandle  // Used to pass out the created task's handle.
  );   

  xReturned = xTaskCreate(
    TaskTemperature,    // Function that implements the task. 
    "Temperature",       // Text name for the task. 
    2000,           // Stack size in words, not bytes. 
    NULL,           // Parameter passed into the task. 
    1,              // Priority at which the task is created. 
    &xTempHandle  // Used to pass out the created task's handle.
  );   

  xReturned = xTaskCreate(
    TaskTempWatchdog,    // Function that implements the task. 
    "Watchdog",       // Text name for the task. 
    2000,           // Stack size in words, not bytes. 
    NULL,           // Parameter passed into the task. 
    1,              // Priority at which the task is created. 
    &xWatchdogHandle  // Used to pass out the created task's handle.
  );   

  xReturned = xTaskCreate(
    TaskMotor,    // Function that implements the task. 
    "Motor",       // Text name for the task. 
    2000,           // Stack size in words, not bytes. 
    NULL,           // Parameter passed into the task. 
    1,              // Priority at which the task is created. 
    &xMotorHandle  // Used to pass out the created task's handle.
  );   

}

void loop() {
  
}
