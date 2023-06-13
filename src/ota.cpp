#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#define ESP_LOGE(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGW(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGI(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGD(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGV(tag, ...) Serial.printf("[ %s ] ", tag); Serial.printf(__VA_ARGS__)
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <Update.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#endif

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "semver_extensions.h"
#include "ota.h"

#ifdef ESP8266
ESP8266HTTPUpdate Updater;
#elif defined(ESP32)
HTTPUpdate Updater;
#endif

// Set time via NTP, as required for x.509 validation
void syncClock()
{
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(100);
    now = time(nullptr);
  }
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
}

GitHubOTA::GitHubOTA(
    String version,
    String release_url,
    String firmware_name,
    String filesystem_name,
    String fs_pending_filename,
    bool fetch_url_via_redirect)
{
  ESP_LOGV("GitHubOTA", "GitHubOTA(version: %s, fetch_url_via_redirect: %d)\n", version.c_str(), fetch_url_via_redirect);

  LittleFS.begin();

  _version = from_string(version.c_str());
  _release_url = release_url;
  _firmware_name = firmware_name;
  _filesystem_name = filesystem_name;
  _fs_pending_filename = fs_pending_filename;
  _fetch_url_via_redirect = fetch_url_via_redirect;

  Updater.rebootOnUpdate(false);
#ifdef ESP32
  _wifi_client.setCACert(github_certificate);
#elif defined(ESP8266)
  // _wifi_client.setInsecure();
  _x509.append(github_certificate);
  _wifi_client.setTrustAnchors(&_x509);
#endif

#ifdef LED_BUILTIN
  Updater.setLedPin(LED_BUILTIN, LOW);
#endif
  Updater.onStart(update_started);
  Updater.onEnd(update_finished);
  Updater.onProgress(update_progress);
  Updater.onError(update_error);
  Updater.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
}

void GitHubOTA::handle()
{
  const char *TAG = "handle";
  syncClock();

  String base_url = _fetch_url_via_redirect ? get_updated_base_url_via_redirect() : get_updated_base_url_via_api();
  ESP_LOGI(TAG, "base_url %s\n", base_url.c_str());

  if (fs_update_pending()){
    auto result = update_filesystem(base_url + _filesystem_name);

    if (result != HTTP_UPDATE_FAILED){
      fs_update_finished();
    }

    ESP_LOGI(TAG, "Restarting...\n");
    ESP.restart();
  } else {
    auto last_slash = base_url.lastIndexOf('/', base_url.length()-2);
    auto semver_str = base_url.substring(last_slash + 1);
    auto _new_version = from_string(semver_str.c_str());

    if (update_required(_new_version)){
      auto result = update_firmware(base_url + _firmware_name);

      if (result != HTTP_UPDATE_FAILED){
        fs_schedule_update();
      }

      ESP_LOGI(TAG, "Restarting...\n");
      ESP.restart();
    }
  }

  ESP_LOGI(TAG, "No updates found\n");
}

HTTPUpdateResult GitHubOTA::update_firmware(String url)
{
  const char *TAG = "update_firmware";
  ESP_LOGI(TAG, "Download URL: %s\n", url.c_str());

  _wifi_client.setInsecure(); // Running OOM when using second certificate
  auto result = Updater.update(_wifi_client, url);

  print_update_result(result, TAG);
  return result;
}

HTTPUpdateResult GitHubOTA::update_filesystem(String url)
{
  const char *TAG = "update_filesystem";
  ESP_LOGI(TAG, "Download URL: %s\n", url.c_str());

#ifdef ESP8266
  _wifi_client.setInsecure(); // Running OOM when using second certificate
  auto result = Updater.updateFS(_wifi_client, url);
#elif defined(ESP32)
  auto result = Updater.updateSpiffs(_wifi_client, url);
#endif

  print_update_result(result, TAG);
  return result;
}

void GitHubOTA::print_update_result(HTTPUpdateResult result, const char *TAG)
{
  switch (result)
  {
    case HTTP_UPDATE_FAILED:
      ESP_LOGI(TAG, "HTTP_UPDATE_FAILED Error (%d): %s\n", Updater.getLastError(), Updater.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      ESP_LOGI(TAG, "HTTP_UPDATE_NO_UPDATES\n");
      break;
    case HTTP_UPDATE_OK:
      ESP_LOGI(TAG, "HTTP_UPDATE_OK\n");
      break;
  }
}

String GitHubOTA::get_updated_base_url_via_api()
{
  const char *TAG = "get_updated_base_url_via_api";
  ESP_LOGI(TAG, "Release_url: %s\n", _release_url.c_str());

  String base_url = "";

  HTTPClient https;
  // https.useHTTP10(true);
  // https.addHeader("Accept-Encoding", "identity");

  if (!https.begin(_wifi_client, _release_url))
  {
    ESP_LOGI(TAG, "[HTTPS] Unable to connect\n");
    return base_url;
  }

  int httpCode = https.GET();
  if (httpCode < 0)
  {
    ESP_LOGI(TAG, "[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
  }
  else if (httpCode >= 400)
  {
    ESP_LOGI(TAG, "[HTTPS] GET... failed, HTTP Status code: %d\n", httpCode);
  }
  else if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
  {
    StaticJsonDocument<64> filter;
    filter["html_url"] = true;
    filter["name"] = true;

    StaticJsonDocument<256> doc;
    auto result = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filter));
    if (result != DeserializationError::Ok) {
      ESP_LOGI(TAG, "deserializeJson error %s\n", result.c_str());
    }

    base_url = String((const char *)doc["html_url"]);
    base_url.replace("tag", "download");
    base_url += "/";
  }

  https.end();
  return base_url;
}

String GitHubOTA::get_updated_base_url_via_redirect()
{
  const char *TAG = "get_updated_base_url_via_redirect";

  String location = get_redirect_location(_release_url);
  ESP_LOGV(TAG, "location: %s\n", location.c_str());

  if (location.length() <= 0)
  {
    ESP_LOGE(TAG, "[HTTPS] No redirect url\n");
    return "";
  }

  String base_url = "";
  base_url = location + "/";
  base_url.replace("tag", "download");

  ESP_LOGV(TAG, "returns: %s\n", base_url.c_str());
  return base_url;
}

String GitHubOTA::get_redirect_location(String initial_url)
{
  const char *TAG = "get_redirect_location";
  ESP_LOGV(TAG, "initial_url: %s\n", initial_url.c_str());

  HTTPClient https;
  https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!https.begin(_wifi_client, initial_url))
  {
    ESP_LOGE(TAG, "[HTTPS] Unable to connect\n");
    return "";
  }

  int httpCode = https.GET();
  ESP_LOGV(TAG, "httpCode = %d\n", httpCode);
  if (httpCode != HTTP_CODE_FOUND)
  {
    ESP_LOGE(TAG, "[HTTPS] GET... failed, No redirect\n");
  }

  String redirect_url = https.getLocation();
  https.end();

  ESP_LOGV(TAG, "returns: %s\n", redirect_url.c_str());
  return redirect_url;
}

bool GitHubOTA::update_required(semver_t _new_version){
  return _new_version > _version;
}

bool GitHubOTA::fs_update_pending() {
  return LittleFS.exists(_fs_pending_filename);
}

void GitHubOTA::fs_schedule_update() {
  File file = LittleFS.open(_fs_pending_filename, "w");
  if (!file) {
    ESP_LOGV("fs_schedule_update", "Failed to open file for writing\n");
    return;
  }

  ESP_LOGI("fs_schedule_update", "schecduled\n");
  delay(2000);
  file.close();
}

void GitHubOTA::fs_update_finished() {
  if (!LittleFS.remove(_fs_pending_filename)) {
    ESP_LOGV("fs_schedule_update", "Could not delete file: %s\n", _fs_pending_filename.c_str());
  }
}

void update_started()
{
  ESP_LOGI("update_started", "HTTP update process started\n");
}

void update_finished()
{
  ESP_LOGI("update_finished", "HTTP update process finished\n");
}

void update_progress(int currentlyReceiced, int totalBytes)
{
  ESP_LOGI("update_progress", "\rData received, Progress: %.2f %%", 100.0 * currentlyReceiced / totalBytes);
}

void update_error(int err)
{
  ESP_LOGI("update_error", "HTTP update fatal error code %d\n", err);
}

const char *github_certificate PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)CERT";