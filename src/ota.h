#ifndef ESP_GHOTA4ARDUINO_H
#define ESP_GHOTA4ARDUINO_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#endif

#define CONTENT_LENGTH_HEADER "content-length"

extern const char *github_certificate;

void init_ota(String version);
void init_ota(String version, bool fetch_url_via_redirect);
void handle_ota(String releaseUrl);

String get_updated_firmware_url_via_api(String releaseUrl, WiFiClientSecure* client);
String get_updated_firmware_url_via_redirect(String releaseUrl, WiFiClientSecure* client);

void update_started();
void update_finished();
void update_progress(int currentyReceiced, int totalBytes);
void update_error(int err);
#endif
