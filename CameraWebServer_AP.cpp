#include "CameraWebServer_AP.h"
#include "esp_system.h"

void startControlServer();

void CameraWebServer_AP::CameraWebServer_AP_Init(void) {
  Serial.setDebugOutput(true);

  uint64_t chipid = ESP.getEfuseMac();
  char buffer[10];

  sprintf(buffer, "%04X", static_cast<uint16_t>(chipid >> 32));
  String mac0 = String(buffer);
  sprintf(buffer, "%08X", static_cast<uint32_t>(chipid));
  String mac1 = String(buffer);

  wifi_name = mac0 + mac1;
  String full_ssid = String(ssid) + wifi_name;

  Serial.println(":----------------------------:");
  Serial.print("wifi_name:");
  Serial.println(full_ssid);
  Serial.println(":----------------------------:");

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(full_ssid.c_str(), password, WIFI_CHANNEL);

  startControlServer();

  Serial.print("Control Ready! Use 'http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("' to connect");
}
