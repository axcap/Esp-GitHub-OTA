#ifndef ESP_GHOTA4ARDUINO_H
#define ESP_GHOTA4ARDUINO_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include "HttpsOTAUpdate.h"
#endif

#define CONTENT_LENGTH_HEADER "content-length"

extern const char *github_certificate;

void init_ota(String version, Stream& serial = Serial);
void handle_ota(String releaseUrl);
String get_updated_firmware_url(String releaseUrl, WiFiClientSecure* client);

void update_started();
void update_finished();
void update_progress(int cur, int total);
void update_error(int err);

#ifdef ESP32
void HttpEvent(HttpEvent_t *event);
#endif

#endif
