#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ctype.h>
#include <Adafruit_NeoPixel.h>
#include <esp_wifi.h>

// ================= LED CONFIG =================
#define LED_PIN        1
#define NUMPIXELS      15

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ================= CONFIG =================
static const uint16_t kHttpPort = 8080;
static const uint32_t kStaConnectTimeoutMs = 20000;
static const char * kPrefsNamespace = "doormount";
static const char * kPrefSsidKey = "ssid";
static const char * kPrefPassKey = "pass";
static const char * kPrefUserIdKey = "uid";
static const char * kLedStatePath = "/api/doormount/led";
static const char * kMdnsHost = "doormount";

// Power knobs (code constants) for battery tuning.
static const uint8_t kLedBrightnessPercent = 30U;
static const uint8_t kApIdleBrightnessPercent = 12U;
static const uint32_t kApIdleDimAfterMs = 120000U;
static const uint16_t kLoopIdleDelayMs = 10U;
static const uint8_t kCpuFrequencyMhz = 80U;
static const bool kStaWifiPowerSaveEnabled = true;
static const bool kDisableSerialAfterBoot = true;

WebServer server(kHttpPort);
Preferences prefs;

String g_savedSsid;
String g_savedPassword;
int32_t g_savedUserId = 0;

String g_apSsid;
bool g_apMode = false;
bool g_mdnsStarted = false;
String g_ledState = "GREEN";
bool g_ledOutputInitialized = false;
bool g_apIdleDimApplied = false;
uint32_t g_lastHttpActivityMs = 0;
uint8_t g_activeBrightnessPercent = kLedBrightnessPercent;

static void renderCurrentLedState(void);

static uint8_t clampPercent(uint8_t percent) {
	return (percent > 100U) ? 100U : percent;
}

static uint8_t scaleChannel(uint8_t channel, uint8_t brightnessPercent) {
	uint16_t scaled = ((uint16_t)channel * (uint16_t)brightnessPercent + 50U) / 100U;
	return (uint8_t)scaled;
}

static void setActiveLedBrightnessPercent(uint8_t brightnessPercent) {
	uint8_t clamped = clampPercent(brightnessPercent);
	if (g_activeBrightnessPercent == clamped) {
		return;
	}

	g_activeBrightnessPercent = clamped;

	if (g_ledOutputInitialized) {
		renderCurrentLedState();
	}
}

static const char * staWifiPowerSaveLabel(void) {
	return kStaWifiPowerSaveEnabled ? "MIN_MODEM" : "NONE";
}

static void applyWifiPowerSavePolicy(void) {
	wifi_ps_type_t psMode = WIFI_PS_NONE;
	if (!g_apMode && kStaWifiPowerSaveEnabled) {
		psMode = WIFI_PS_MIN_MODEM;
	}

	esp_wifi_set_ps(psMode);
}

static void markHttpActivity(void) {
	g_lastHttpActivityMs = millis();

	if (g_apMode && g_apIdleDimApplied) {
		g_apIdleDimApplied = false;
		setActiveLedBrightnessPercent(kLedBrightnessPercent);
	}
}

static void applyApIdlePowerPolicy(void) {
	if (!g_apMode || g_apIdleDimApplied) {
		return;
	}

	if ((uint32_t)(millis() - g_lastHttpActivityMs) >= kApIdleDimAfterMs) {
		g_apIdleDimApplied = true;
		setActiveLedBrightnessPercent(kApIdleBrightnessPercent);
	}
}

static void maybeDisableSerial(void) {
	if (!kDisableSerialAfterBoot) {
		return;
	}

	Serial.flush();
	Serial.end();
}

// ================= LED FUNCTIONS =================
static void setLedsColor(uint8_t red, uint8_t green, uint8_t blue) {
	uint8_t scaledRed = scaleChannel(red, g_activeBrightnessPercent);
	uint8_t scaledGreen = scaleChannel(green, g_activeBrightnessPercent);
	uint8_t scaledBlue = scaleChannel(blue, g_activeBrightnessPercent);

	for (int i = 0; i < NUMPIXELS; i++) {
		pixels.setPixelColor(i, pixels.Color(scaledRed, scaledGreen, scaledBlue));
	}
	pixels.show();
	g_ledOutputInitialized = true;
}

static void setLedsRed() {
	setLedsColor(255, 0, 0);
}

static void setLedsYellow() {
	setLedsColor(255, 180, 0);
}

static void setLedsGreen() {
	setLedsColor(0, 255, 0);
}

static void renderCurrentLedState(void) {
	if (g_ledState == "RED") {
		setLedsRed();
		return;
	}

	if (g_ledState == "YELLOW") {
		setLedsYellow();
		return;
	}

	setLedsGreen();
}

static bool applyLedState(const String & state) {
	if (state != "RED" && state != "YELLOW" && state != "GREEN") {
		return false;
	}

	bool stateChanged = (g_ledState != state);
	g_ledState = state;

	if (stateChanged || !g_ledOutputInitialized) {
		renderCurrentLedState();
	}

	return true;
}

// ================= HELPERS =================
static String uppercaseHex4(uint16_t value) {
	char buf[5] = {0};
	snprintf(buf, sizeof(buf), "%04X", value);
	return String(buf);
}

static bool extractJsonString(const String & body, const char * key, String & outValue) {
	String needle = String("\"") + key + "\"";
	int keyPos = body.indexOf(needle);
	if (keyPos < 0) return false;

	int colonPos = body.indexOf(':', keyPos + needle.length());
	if (colonPos < 0) return false;

	int startQuote = body.indexOf('"', colonPos + 1);
	if (startQuote < 0) return false;

	int endQuote = -1;
	for (int i = startQuote + 1; i < (int)body.length(); i++) {
		if (body.charAt(i) == '"' && body.charAt(i - 1) != '\\') {
			endQuote = i;
			break;
		}
	}

	if (endQuote < 0 || endQuote <= startQuote + 1) return false;

	outValue = body.substring(startQuote + 1, endQuote);
	return true;
}

static bool extractJsonInt(const String & body, const char * key, int32_t & outValue) {
	String needle = String("\"") + key + "\"";
	int keyPos = body.indexOf(needle);
	if (keyPos < 0) return false;

	int colonPos = body.indexOf(':', keyPos + needle.length());
	if (colonPos < 0) return false;

	int cursor = colonPos + 1;
	while (cursor < (int)body.length() && isspace((unsigned char)body.charAt(cursor))) {
		cursor++;
	}

	if (cursor >= (int)body.length()) return false;

	int end = cursor;
	while (end < (int)body.length()) {
		char c = body.charAt(end);
		if (!(c == '-' || (c >= '0' && c <= '9'))) break;
		end++;
	}

	if (end <= cursor) return false;

	String numberToken = body.substring(cursor, end);
	outValue = (int32_t)numberToken.toInt();
	return true;
}

// ================= STORAGE =================
static void loadStoredConfig() {
	prefs.begin(kPrefsNamespace, true);
	g_savedSsid = prefs.getString(kPrefSsidKey, "");
	g_savedPassword = prefs.getString(kPrefPassKey, "");
	g_savedUserId = prefs.getInt(kPrefUserIdKey, 0);
	prefs.end();
}

static bool saveStoredConfig(const String & ssid, const String & password, int32_t userId) {
	prefs.begin(kPrefsNamespace, false);
	bool ok = prefs.putString(kPrefSsidKey, ssid) > 0;
	ok = ok && (prefs.putString(kPrefPassKey, password) > 0);
	prefs.putInt(kPrefUserIdKey, userId);
	prefs.end();

	if (ok) {
		g_savedSsid = ssid;
		g_savedPassword = password;
		g_savedUserId = userId;
	}

	return ok;
}

static bool hasStoredCredentials() {
	return g_savedSsid.length() > 0 && g_savedPassword.length() >= 8;
}

// ================= WIFI =================
static bool connectToHomeWifi(const String & ssid, const String & password, uint32_t timeoutMs) {
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(false);
	WiFi.begin(ssid.c_str(), password.c_str());
	applyWifiPowerSavePolicy();

	uint32_t startMs = millis();
	while ((millis() - startMs) < timeoutMs) {
		if (WiFi.status() == WL_CONNECTED) {
			Serial.print("[WiFi] Connected. IP: ");
			Serial.println(WiFi.localIP());

			setLedsGreen();
			applyWifiPowerSavePolicy();

			return true;
		}
		delay(250);
	}

	Serial.println("[WiFi] Station connect timed out.");
	WiFi.disconnect(true, true);
	return false;
}

static void stopMdnsIfRunning() {
	if (g_mdnsStarted) {
		MDNS.end();
		g_mdnsStarted = false;
	}
}

static void startMdns(void) {
	stopMdnsIfRunning();

	if (MDNS.begin(kMdnsHost)) {
		MDNS.addService("http", "tcp", kHttpPort);
		g_mdnsStarted = true;
		Serial.print("[mDNS] Registered host: ");
		Serial.println(String(kMdnsHost) + ".local");
	} else {
		Serial.println("[mDNS] Failed to start mDNS responder.");
	}
}

// ================= HTTP =================
static void respondJson(int code, const String & json) {
	server.send(code, "application/json", json);
}

static void handleHealth() {
	markHttpActivity();

	String mode = g_apMode ? "ap" : "sta";
	String staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("");
	String body = String("{\"status\":\"ok\",\"mode\":\"") + mode +
				  "\",\"apSsid\":\"" + g_apSsid +
				  "\",\"ledState\":\"" + g_ledState +
				  "\",\"mdnsHost\":\"" + kMdnsHost +
				  ".local\",\"staIp\":\"" + staIp +
				  "\",\"brightnessPct\":" + String(g_activeBrightnessPercent) +
				  ",\"apIdleBrightnessPct\":" + String(kApIdleBrightnessPercent) +
				  ",\"apIdleDimAfterMs\":" + String(kApIdleDimAfterMs) +
				  ",\"loopDelayMs\":" + String(kLoopIdleDelayMs) +
				  ",\"cpuFreqMhz\":" + String(kCpuFrequencyMhz) +
				  ",\"staWifiPowerSave\":\"" + String(staWifiPowerSaveLabel()) +
				  "\",\"apIdleDimmed\":" + String(g_apIdleDimApplied ? "true" : "false") +
				  "}";
	respondJson(200, body);
}

static void handleLedState() {
	markHttpActivity();

	if (!server.hasArg("plain")) {
		respondJson(400, "{\"status\":\"error\",\"message\":\"missing body\"}");
		return;
	}

	String body = server.arg("plain");
	String stateValue;
	if (!extractJsonString(body, "state", stateValue)) {
		respondJson(400, "{\"status\":\"error\",\"message\":\"invalid payload\"}");
		return;
	}

	stateValue.trim();
	stateValue.toUpperCase();

	if (!applyLedState(stateValue)) {
		respondJson(400, "{\"status\":\"error\",\"message\":\"unknown state\"}");
		return;
	}

	respondJson(200, String("{\"status\":\"ok\",\"state\":\"") + g_ledState + "\"}");
}

static void handleDoormountSetup() {
	markHttpActivity();

	if (!server.hasArg("plain")) {
		respondJson(400, "{\"status\":\"error\",\"message\":\"missing body\"}");
		return;
	}

	String body = server.arg("plain");
	String wifiSsid;
	String wifiPassword;
	int32_t userId = 0;

	bool hasSsid = extractJsonString(body, "wifiSsid", wifiSsid);
	bool hasPassword = extractJsonString(body, "wifiPassword", wifiPassword);
	bool hasUserId = extractJsonInt(body, "userId", userId);

	if (!hasSsid || !hasPassword || !hasUserId) {
		respondJson(400, "{\"status\":\"error\",\"message\":\"invalid payload\"}");
		return;
	}

	wifiSsid.trim();
	wifiPassword.trim();

	if (wifiSsid.length() < 1 || wifiSsid.length() > 32 ||
		wifiPassword.length() < 8 || wifiPassword.length() > 63 ||
		userId <= 0) {
		respondJson(400, "{\"status\":\"error\",\"message\":\"payload constraints failed\"}");
		return;
	}

	if (!saveStoredConfig(wifiSsid, wifiPassword, userId)) {
		respondJson(500, "{\"status\":\"error\",\"message\":\"save failed\"}");
		return;
	}

	respondJson(200, "{\"status\":\"accepted\",\"message\":\"saved_rebooting\"}");
	delay(400);

	server.stop();
	WiFi.softAPdisconnect(true);
	delay(200);
	ESP.restart();
}

static void handleNotFound() {
	markHttpActivity();
	respondJson(404, "{\"status\":\"error\",\"message\":\"not found\"}");
}

static void configureRoutes() {
	server.on("/health", HTTP_GET, handleHealth);
	server.on("/api/doormount/setup", HTTP_POST, handleDoormountSetup);
	server.on(kLedStatePath, HTTP_POST, handleLedState);
	server.onNotFound(handleNotFound);
}

// ================= AP MODE =================
static void startProvisioningAp() {
	uint16_t suffix = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
	g_apSsid = String("DoorMount-") + uppercaseHex4(suffix);

	IPAddress localIp(192, 168, 4, 1);
	IPAddress gateway(192, 168, 4, 1);
	IPAddress subnet(255, 255, 255, 0);

	stopMdnsIfRunning();

	WiFi.disconnect(true, true);
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(localIp, gateway, subnet);

	WiFi.softAP(g_apSsid.c_str());

	g_apMode = true;
	g_lastHttpActivityMs = millis();
	g_apIdleDimApplied = false;
	setActiveLedBrightnessPercent(kLedBrightnessPercent);
	applyWifiPowerSavePolicy();

	configureRoutes();
	server.begin();

	Serial.println("[AP] Started: " + g_apSsid);
	Serial.println("[HTTP] Provisioning server listening on :8080");
}

// ================= SETUP =================
void setup() {
	Serial.begin(115200);
	delay(500);

	setCpuFrequencyMhz(kCpuFrequencyMhz);

	pixels.begin();
	pixels.clear();
	pixels.show();
	setActiveLedBrightnessPercent(kLedBrightnessPercent);
	g_lastHttpActivityMs = millis();

	Serial.println();
	Serial.println("=== DoorMount ESP32-C3 Boot ===");

	loadStoredConfig();

	if (hasStoredCredentials()) {
		Serial.println("[Boot] Stored SSID found: " + g_savedSsid);

		if (connectToHomeWifi(g_savedSsid, g_savedPassword, kStaConnectTimeoutMs)) {
			g_apMode = false;
			g_apSsid = "";
			setActiveLedBrightnessPercent(kLedBrightnessPercent);
			g_apIdleDimApplied = false;
			configureRoutes();
			server.begin();
			startMdns();
			Serial.println("[HTTP] Runtime server listening on :8080");
			Serial.println("[Boot] Running in STA mode.");
			maybeDisableSerial();
			return;
		}

		Serial.println("[Boot] STA connect failed, switching to provisioning AP.");
	} else {
		Serial.println("[Boot] No saved credentials, starting provisioning AP.");
	}

	startProvisioningAp();
	maybeDisableSerial();
}

// ================= LOOP =================
void loop() {
	server.handleClient();
	applyApIdlePowerPolicy();
	delay(kLoopIdleDelayMs);
}
