/*
 * @Descripttion: 
 * @version: 
 * @Author: Elegoo
 * @Date: 2020-06-04 11:42:27
 * @LastEditors: Changhua
 * @LastEditTime: 2020-07-23 14:21:48
 */

#ifndef _CONTROL_NETWORK_H
#define _CONTROL_NETWORK_H
#include <WiFi.h>
#include "config.h"

void initControlNetworkSettings();
String getControlNetworkName();
String getControlHostName();
IPAddress getControlIpAddress();
int getControlClientCount();
bool isControlApMode();
String getConfiguredNetworkMode();
String getConfiguredStaSsid();
String getConfiguredApSsid();
String getConfiguredStaPassword();
String getConfiguredApPassword();
bool saveControlNetworkSettings(
  const String &mode,
  const String &staSsid,
  const String &staPassword,
  const String &apSsid,
  const String &apPassword,
  String &message);
bool toggleControlNetworkMode(String &message);

class ControlNetwork
{

public:
  void ControlNetwork_Init(void);
  String wifi_name;
};

#endif
