#include <WiFi.h>
#include <WebServer.h>

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

enum temperature_profile current_profile = COLD;

struct temperatures {
  int low;
  int high;
};

const struct temperatures temperature_profiles[] = { 
  { 50, 60 }, // COLD
  { 60, 70 }, // MIDDLE
  { 70, 80 }  // HOT
};

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

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi network
  

  BaseType_t xReturned;
  
  TaskHandle_t xServerHandle = NULL;
  xReturned = xTaskCreate(
    TaskServer,    // Function that implements the task. 
    "Server",       // Text name for the task. 
    5000,           // Stack size in words, not bytes. 
    NULL,           // Parameter passed into the task. 
    1,              // Priority at which the task is created. 
    &xServerHandle  // Used to pass out the created task's handle.
  );   

}

void loop() {
  
}
