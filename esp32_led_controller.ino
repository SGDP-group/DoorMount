#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// LED pin configuration (GPIO pins for RGB LED)
const int RED_PIN = 25;     // GPIO 25 for red
const int GREEN_PIN = 26;   // GPIO 26 for green
const int BLUE_PIN = 27;    // GPIO 27 for blue

// PWM settings
const int PWM_FREQUENCY = 5000;
const int PWM_RESOLUTION = 8;

// Web server on port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize LED pins as output with PWM
  ledcSetup(0, PWM_FREQUENCY, PWM_RESOLUTION);  // Red channel
  ledcSetup(1, PWM_FREQUENCY, PWM_RESOLUTION);  // Green channel
  ledcSetup(2, PWM_FREQUENCY, PWM_RESOLUTION);  // Blue channel
  
  ledcAttachPin(RED_PIN, 0);
  ledcAttachPin(GREEN_PIN, 1);
  ledcAttachPin(BLUE_PIN, 2);
  
  // Turn off LED initially
  setColor(0, 0, 0);
  
  Serial.println("\n\nStarting ESP32 LED Controller...");
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/color", HTTP_POST, handleColorChange);
  server.on("/flash", HTTP_POST, handleFlash);
  server.on("/status", HTTP_GET, handleStatus);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  delay(1);
}

/**
 * Connect to WiFi network
 */
void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

/**
 * Set LED color using RGB values (0-255)
 */
void setColor(int red, int green, int blue) {
  ledcWrite(0, red);
  ledcWrite(1, green);
  ledcWrite(2, blue);
  
  Serial.printf("LED Color set to RGB(%d, %d, %d)\n", red, green, blue);
}

/**
 * Flash LED with specified color
 */
void flashColor(int red, int green, int blue, int flashes, int duration) {
  for (int i = 0; i < flashes; i++) {
    setColor(red, green, blue);
    delay(duration);
    setColor(0, 0, 0);
    delay(duration);
  }
}

/**
 * Handle root path - return API documentation
 */
void handleRoot() {
  String html = R"(
    <html>
      <head>
        <title>ESP32 LED Controller API</title>
        <style>
          body { font-family: Arial; margin: 20px; background-color: #f0f0f0; }
          .container { background-color: white; padding: 20px; border-radius: 5px; }
          h1 { color: #333; }
          .endpoint { background-color: #f5f5f5; padding: 15px; margin: 10px 0; border-left: 4px solid #007bff; }
          code { background-color: #e9ecef; padding: 2px 5px; border-radius: 3px; }
          .method { color: #007bff; font-weight: bold; }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>ESP32 LED Controller API</h1>
          <p>Controls RGB LED via HTTP API calls</p>
          
          <div class="endpoint">
            <h3><span class="method">POST</span> /color</h3>
            <p>Set LED to a specific color</p>
            <p><strong>Parameters (JSON):</strong></p>
            <code>{"red": 255, "green": 0, "blue": 0}</code>
            <p><strong>Example:</strong> Red LED</p>
          </div>
          
          <div class="endpoint">
            <h3><span class="method">POST</span> /flash</h3>
            <p>Flash LED with specified color</p>
            <p><strong>Parameters (JSON):</strong></p>
            <code>{"red": 0, "green": 255, "blue": 0, "flashes": 3, "duration": 500}</code>
            <p><strong>Example:</strong> Green flash 3 times, 500ms per flash</p>
          </div>
          
          <div class="endpoint">
            <h3><span class="method">GET</span> /status</h3>
            <p>Get current LED status and IP address</p>
          </div>
          
          <h2>Color Presets</h2>
          <ul>
            <li><strong>Red:</strong> {red: 255, green: 0, blue: 0}</li>
            <li><strong>Green:</strong> {red: 0, green: 255, blue: 0}</li>
            <li><strong>Blue:</strong> {red: 0, green: 0, blue: 255}</li>
            <li><strong>Yellow:</strong> {red: 255, green: 255, blue: 0}</li>
            <li><strong>Cyan:</strong> {red: 0, green: 255, blue: 255}</li>
            <li><strong>Magenta:</strong> {red: 255, green: 0, blue: 255}</li>
            <li><strong>White:</strong> {red: 255, green: 255, blue: 255}</li>
            <li><strong>Off:</strong> {red: 0, green: 0, blue: 0}</li>
          </ul>
        </div>
      </body>
    </html>
  )";
  
  server.send(200, "text/html", html);
}

/**
 * Handle /color endpoint - set LED to specific color
 */
void handleColorChange() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Parse JSON
    int red = 0, green = 0, blue = 0;
    
    if (body.indexOf("\"red\"") != -1) {
      red = extractValue(body, "red");
    }
    if (body.indexOf("\"green\"") != -1) {
      green = extractValue(body, "green");
    }
    if (body.indexOf("\"blue\"") != -1) {
      blue = extractValue(body, "blue");
    }
    
    // Constrain values to 0-255
    red = constrain(red, 0, 255);
    green = constrain(green, 0, 255);
    blue = constrain(blue, 0, 255);
    
    // Set color
    setColor(red, green, blue);
    
    // Send response
    String response = "{\"status\": \"success\", \"red\": " + String(red) + 
                      ", \"green\": " + String(green) + ", \"blue\": " + String(blue) + "}";
    server.send(200, "application/json", response);
  } else {
    server.send(400, "application/json", "{\"error\": \"No JSON body provided\"}");
  }
}

/**
 * Handle /flash endpoint - flash LED with specified color
 */
void handleFlash() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    int red = extractValue(body, "red");
    int green = extractValue(body, "green");
    int blue = extractValue(body, "blue");
    int flashes = extractValue(body, "flashes");
    int duration = extractValue(body, "duration");
    
    // Set defaults if not provided
    if (flashes == 0) flashes = 1;
    if (duration == 0) duration = 500;
    
    // Constrain values
    red = constrain(red, 0, 255);
    green = constrain(green, 0, 255);
    blue = constrain(blue, 0, 255);
    flashes = constrain(flashes, 1, 100);
    duration = constrain(duration, 50, 5000);
    
    // Flash the LED (non-blocking version would be better for production)
    flashColor(red, green, blue, flashes, duration);
    
    String response = "{\"status\": \"flashed\", \"red\": " + String(red) + 
                      ", \"green\": " + String(green) + ", \"blue\": " + String(blue) + 
                      ", \"flashes\": " + String(flashes) + ", \"duration\": " + String(duration) + "}";
    server.send(200, "application/json", response);
  } else {
    server.send(400, "application/json", "{\"error\": \"No JSON body provided\"}");
  }
}

/**
 * Handle /status endpoint - return device status
 */
void handleStatus() {
  String response = "{\"ip\": \"" + WiFi.localIP().toString() + 
                    "\", \"ssid\": \"" + String(ssid) + 
                    "\", \"signal_strength\": " + String(WiFi.RSSI()) + 
                    ", \"status\": \"online\"}";
  server.send(200, "application/json", response);
}

/**
 * Simple JSON value extractor for integer values
 */
int extractValue(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int startIndex = json.indexOf(searchKey);
  
  if (startIndex == -1) return 0;
  
  startIndex += searchKey.length();
  int endIndex = json.indexOf(",", startIndex);
  if (endIndex == -1) {
    endIndex = json.indexOf("}", startIndex);
  }
  
  String valueStr = json.substring(startIndex, endIndex);
  valueStr.trim();
  
  return valueStr.toInt();
}
