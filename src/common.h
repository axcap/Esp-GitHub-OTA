#ifndef GITHUBOTA_COMMON_H
#define GITHUBOTA_COMMON_H

#ifdef ESP8266
#define ESP_LOGE(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGW(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGI(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGD(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGV(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <Update.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#endif

#ifdef ESP8266
typedef ESP8266HTTPUpdate Updater;
#elif defined(ESP32)
typedef HTTPUpdate Updater;
#endif

#include "semver.h"

String get_updated_base_url_via_api(WiFiClientSecure wifi_client, String release_url);
String get_updated_base_url_via_redirect(WiFiClientSecure wifi_client, String release_url);
String get_redirect_location(WiFiClientSecure wifi_client, String initial_url);

void print_update_result(Updater updater, HTTPUpdateResult result, const char *TAG);

bool update_required(semver_t _new_version, semver_t _current_version);

void update_started();
void update_finished();
void update_progress(int currentlyReceiced, int totalBytes);
void update_error(int err);
void synchronize_system_time();

extern const char *github_certificate;

#endif