#include "ControlNetwork.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include "esp_system.h"

void startControlServer();

namespace {

Preferences g_preferences;
bool g_control_ap_mode = true;
String g_control_network_name;
String g_control_host_name = WIFI_HOSTNAME;
String g_unique_suffix;
String g_preferred_mode = "sta";
String g_sta_ssid = WIFI_STA_SSID;
String g_sta_password = WIFI_STA_PWD;
String g_ap_ssid = WIFI_AP_SSID_PREFIX;
String g_ap_password = WIFI_AP_PWD;

String trimCopy(const String &value) {
  String trimmed = value;
  trimmed.trim();
  return trimmed;
}

void stopMdnsIfRunning() {
  if (!g_control_ap_mode) {
    MDNS.end();
  }
}

String buildFallbackApName() {
  if (g_ap_ssid.length() > 0) {
    return g_ap_ssid;
  }
  return String(WIFI_AP_SSID_PREFIX) + g_unique_suffix;
}

void loadPreferences() {
  g_preferences.begin("network", true);
  g_preferred_mode = trimCopy(g_preferences.getString("mode", "sta"));
  g_sta_ssid = g_preferences.getString("sta_ssid", WIFI_STA_SSID);
  g_sta_password = g_preferences.getString("sta_pwd", WIFI_STA_PWD);
  g_ap_ssid = g_preferences.getString("ap_ssid", WIFI_AP_SSID_PREFIX);
  g_ap_password = g_preferences.getString("ap_pwd", WIFI_AP_PWD);
  g_preferences.end();

  if (g_preferred_mode != "ap" && g_preferred_mode != "sta") {
    g_preferred_mode = "sta";
  }
}

void logConnectionSummary() {
  const IPAddress ip = getControlIpAddress();
  const char *mode_label = g_control_ap_mode
    ? (g_preferred_mode == "ap" ? "AP" : "AP fallback")
    : "WiFi STA";
  Serial.println(":----------------------------:");
  Serial.print("network_mode:");
  Serial.println(mode_label);
  Serial.print("network_name:");
  Serial.println(g_control_network_name);
  Serial.print("ip:");
  Serial.println(ip);
  if (!g_control_ap_mode) {
    Serial.print("bonjour_name:http://");
    Serial.print(getControlHostName());
    Serial.println(".local/");
  }
  Serial.println(":----------------------------:");
}

bool connectToWifiStation(const char *ssid, const char *password) {
  if (ssid == NULL || ssid[0] == '\0') {
    Serial.println("STA Wi-Fi SSID is empty, skipping station mode");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(ssid, password);

  Serial.printf("Connecting to Wi-Fi SSID '%s'", ssid);
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to join Wi-Fi within timeout");
    WiFi.disconnect(true, true);
    return false;
  }

  g_control_ap_mode = false;
  g_control_network_name = WiFi.SSID();

  if (!MDNS.begin(WIFI_HOSTNAME)) {
    Serial.println("mDNS start failed");
  } else {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS ready at http://%s.local/\n", WIFI_HOSTNAME);
  }

  return true;
}

void startFallbackAccessPoint(const String &fallbackSsid, const char *password) {
  stopMdnsIfRunning();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(fallbackSsid.c_str(), password, WIFI_CHANNEL);
  g_control_ap_mode = true;
  g_control_network_name = fallbackSsid;
}

}  // namespace

void initControlNetworkSettings() {
  loadPreferences();
}

String getControlNetworkName() {
  return g_control_network_name;
}

String getControlHostName() {
  return g_control_host_name;
}

IPAddress getControlIpAddress() {
  return g_control_ap_mode ? WiFi.softAPIP() : WiFi.localIP();
}

int getControlClientCount() {
  if (g_control_ap_mode) {
    return WiFi.softAPgetStationNum();
  }
  return WiFi.status() == WL_CONNECTED ? 1 : 0;
}

bool isControlApMode() {
  return g_control_ap_mode;
}

String getConfiguredNetworkMode() {
  return g_preferred_mode;
}

String getConfiguredStaSsid() {
  return g_sta_ssid;
}

String getConfiguredApSsid() {
  return buildFallbackApName();
}

String getConfiguredStaPassword() {
  return g_sta_password;
}

String getConfiguredApPassword() {
  return g_ap_password;
}

bool saveControlNetworkSettings(
  const String &mode,
  const String &staSsid,
  const String &staPassword,
  const String &apSsid,
  const String &apPassword,
  String &message) {
  const String normalizedMode = trimCopy(mode);
  const String trimmedStaSsid = trimCopy(staSsid);
  const String trimmedApSsid = trimCopy(apSsid);

  if (normalizedMode != "sta" && normalizedMode != "ap") {
    message = "Mode must be 'sta' or 'ap'.";
    return false;
  }

  if (normalizedMode == "sta" && trimmedStaSsid.length() == 0) {
    message = "STA SSID is required when STA mode is selected.";
    return false;
  }

  if (trimmedApSsid.length() == 0) {
    message = "AP SSID is required.";
    return false;
  }

  const String nextStaPassword = staPassword.length() == 0 ? g_sta_password : staPassword;
  const String nextApPassword = apPassword.length() == 0 ? g_ap_password : apPassword;

  g_preferences.begin("network", false);
  g_preferences.putString("mode", normalizedMode);
  g_preferences.putString("sta_ssid", trimmedStaSsid);
  g_preferences.putString("sta_pwd", nextStaPassword);
  g_preferences.putString("ap_ssid", trimmedApSsid);
  g_preferences.putString("ap_pwd", nextApPassword);
  g_preferences.end();

  g_preferred_mode = normalizedMode;
  g_sta_ssid = trimmedStaSsid;
  g_sta_password = nextStaPassword;
  g_ap_ssid = trimmedApSsid;
  g_ap_password = nextApPassword;
  message = "Network settings saved. Rebooting to apply.";
  return true;
}

bool toggleControlNetworkMode(String &message) {
  const String nextMode = isControlApMode() ? "sta" : "ap";
  return saveControlNetworkSettings(
    nextMode,
    getConfiguredStaSsid(),
    getConfiguredStaPassword(),
    getConfiguredApSsid(),
    getConfiguredApPassword(),
    message);
}

void ControlNetwork::ControlNetwork_Init(void) {
  Serial.setDebugOutput(true);

  uint64_t chipid = ESP.getEfuseMac();
  char buffer[10];

  sprintf(buffer, "%04X", static_cast<uint16_t>(chipid >> 32));
  String mac0 = String(buffer);
  sprintf(buffer, "%08X", static_cast<uint32_t>(chipid));
  String mac1 = String(buffer);

  wifi_name = mac0 + mac1;
  g_unique_suffix = wifi_name;
  initControlNetworkSettings();
  const String fallback_ssid = buildFallbackApName();

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  bool sta_connected = false;
  if (g_preferred_mode == "sta") {
    sta_connected = connectToWifiStation(g_sta_ssid.c_str(), g_sta_password.c_str());
  }

  if (!sta_connected) {
    startFallbackAccessPoint(fallback_ssid, g_ap_password.c_str());
  }

  startControlServer();

  logConnectionSummary();
  Serial.print("Control Ready! Use 'http://");
  if (isControlApMode()) {
    Serial.print(getControlIpAddress());
  } else {
    Serial.print(getControlHostName());
    Serial.print(".local");
  }
  Serial.println("' to connect");
}
