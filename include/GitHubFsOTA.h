#ifndef ESP_GITHUBFS_OTA_H
#define ESP_GITHUBFS_OTA_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#endif

#include "semver.h"

class GitHubFsOTA
{
public:
  GitHubFsOTA(
      String version,
      String release_url,
      String filesystem_name = "filesystem.bin",
      bool fetch_url_via_redirect = false);

  void handle();

private:
#ifdef ESP8266
  ESP8266HTTPUpdate Updater;
#elif defined(ESP32)
  HTTPUpdate Updater;
#endif

  HTTPUpdateResult update_filesystem(String url);

  semver_t _version;
  String _release_url;
  String _filesystem_name;
  bool _fetch_url_via_redirect;
  WiFiClientSecure _wifi_client;
#ifdef ESP8266
  X509List _x509;
#endif
};

#endif
