#ifndef ESP_GITHUB_OTA_H
#define ESP_GITHUB_OTA_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#endif

#include "semver.h"

class GitHubOTA
{
public:
  GitHubOTA(
      String version,
      String release_url,
      String firmware_name = "firmware.bin",
      String filesystem_name = "filesystem.bin",
      String fs_pending_filename = "/.fs_update_pending",
      bool fetch_url_via_redirect = false);

  void handle();

private:
  HTTPUpdateResult update_firmware(String url);
  HTTPUpdateResult update_filesystem(String url);
  void print_update_result(HTTPUpdateResult result, const char *TAG);

  bool update_required(semver_t new_version);
  String get_updated_base_url_via_api();
  String get_updated_base_url_via_redirect();
  String get_redirect_location(String initial_url);
  void fs_schedule_update();
  bool fs_update_pending();
  void fs_update_finished();

  semver_t _version;
  String _release_url;
  String _firmware_name;
  String _filesystem_name;
  String _fs_pending_filename;
  bool _fetch_url_via_redirect;
  WiFiClientSecure _wifi_client;
#ifdef ESP8266
  X509List _x509;
#endif
};

void update_started();
void update_finished();
void update_progress(int currentyReceiced, int totalBytes);
void update_error(int err);

extern const char *github_certificate;

#endif
