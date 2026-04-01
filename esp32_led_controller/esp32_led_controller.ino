#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ctype.h>
#include <Adafruit_NeoPixel.h>

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

WebServer server(kHttpPort);
Preferences prefs;

String g_savedSsid;
String g_savedPassword;
int32_t g_savedUserId = 0;

String g_apSsid;
bool g_apMode = false;

// ================= LED FUNCTIONS =================
static void setLedsRed() {
	for (int i = 0; i < NUMPIXELS; i++) {
		pixels.setPixelColor(i, pixels.Color(255, 0, 0));
	}
	pixels.show();
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

	uint32_t startMs = millis();
	while ((millis() - startMs) < timeoutMs) {
		if (WiFi.status() == WL_CONNECTED) {
			Serial.print("[WiFi] Connected. IP: ");
			Serial.println(WiFi.localIP());

			setLedsRed();  // 🔴 TURN LEDs RED ON SUCCESS

			return true;
		}
		delay(250);
	}

	Serial.println("[WiFi] Station connect timed out.");
	WiFi.disconnect(true, true);
	return false;
}

// ================= HTTP =================
static void respondJson(int code, const String & json) {
	server.send(code, "application/json", json);
}

static void handleHealth() {
	String mode = g_apMode ? "ap" : "sta";
	String body = String("{\"status\":\"ok\",\"mode\":\"") + mode + "\",\"apSsid\":\"" + g_apSsid + "\"}";
	respondJson(200, body);
}

static void handleDoormountSetup() {
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
	respondJson(404, "{\"status\":\"error\",\"message\":\"not found\"}");
}

static void configureRoutes() {
	server.on("/health", HTTP_GET, handleHealth);
	server.on("/api/doormount/setup", HTTP_POST, handleDoormountSetup);
	server.onNotFound(handleNotFound);
}

// ================= AP MODE =================
static void startProvisioningAp() {
	uint16_t suffix = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
	g_apSsid = String("DoorMount-") + uppercaseHex4(suffix);

	IPAddress localIp(192, 168, 4, 1);
	IPAddress gateway(192, 168, 4, 1);
	IPAddress subnet(255, 255, 255, 0);

	WiFi.disconnect(true, true);
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(localIp, gateway, subnet);

	WiFi.softAP(g_apSsid.c_str());

	configureRoutes();
	server.begin();
	g_apMode = true;

	Serial.println("[AP] Started: " + g_apSsid);
	Serial.println("[HTTP] Provisioning server listening on :8080");
}

// ================= SETUP =================
void setup() {
	Serial.begin(115200);
	delay(500);

	pixels.begin();
	pixels.clear();
	pixels.show();

	Serial.println();
	Serial.println("=== DoorMount ESP32-C3 Boot ===");

	loadStoredConfig();

	if (hasStoredCredentials()) {
		Serial.println("[Boot] Stored SSID found: " + g_savedSsid);

		if (connectToHomeWifi(g_savedSsid, g_savedPassword, kStaConnectTimeoutMs)) {
			g_apMode = false;
			Serial.println("[Boot] Running in STA mode.");
			return;
		}

		Serial.println("[Boot] STA connect failed, switching to provisioning AP.");
	} else {
		Serial.println("[Boot] No saved credentials, starting provisioning AP.");
	}

	startProvisioningAp();
}

// ================= LOOP =================
void loop() {
	if (g_apMode) {
		server.handleClient();
	}
}