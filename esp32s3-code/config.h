// config.h
#ifndef CONFIG_H
#define CONFIG_H



// Join this Wi-Fi first on boot.
#define WIFI_STA_SSID ""
#define WIFI_STA_PWD ""

// Bonjour / mDNS hostname when joined to your real Wi-Fi.
// Reach the robot at: http://robotcar.local/
#define WIFI_HOSTNAME "robotcar"

// How long to wait for Wi-Fi before falling back to AP mode.
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Fallback AP settings if STA join fails.
// Final AP name becomes: WIFI_AP_SSID_PREFIX + unique chip id suffix
#define WIFI_AP_SSID_PREFIX "UMRT-CAR-"
// Wifi pwd: "" for no password
#define WIFI_AP_PWD ""

// Wifi Channel: 1-13 generally
// 4 and 5 are confirmed fast - 10 is a little slow
#define WIFI_CHANNEL 4



#endif
