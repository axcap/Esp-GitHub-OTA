#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <Update.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#endif

#include <ArduinoJson.h>
#include "semver_extensions.h"
#include "GitHubOTA.h"
#include "common.h"

GitHubOTA::GitHubOTA(
    String version,
    String release_url,
    String firmware_name,
    bool fetch_url_via_redirect)
{
  ESP_LOGV("GitHubOTA", "GitHubOTA(version: %s, firmware_name: %s, fetch_url_via_redirect: %d)\n",
           version.c_str(), firmware_name.c_str(), fetch_url_via_redirect);

  _version = from_string(version.c_str());
  _release_url = release_url;
  _firmware_name = firmware_name;
  _fetch_url_via_redirect = fetch_url_via_redirect;

  Updater.rebootOnUpdate(false);
#ifdef ESP8266
  _x509.append(github_certificate);
  _wifi_client.setTrustAnchors(&_x509);
#elif defined(ESP32)
  _wifi_client.setCACert(github_certificate);
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
  synchronize_system_time();

  String base_url = _fetch_url_via_redirect ?
    get_updated_base_url_via_redirect(_wifi_client, _release_url) :
    get_updated_base_url_via_api(_wifi_client, _release_url);
  ESP_LOGI(TAG, "base_url %s\n", base_url.c_str());

  auto last_slash = base_url.lastIndexOf('/', base_url.length() - 2);
  auto semver_str = base_url.substring(last_slash + 1);
  auto _new_version = from_string(semver_str.c_str());

  if (update_required(_new_version, _version))
  {
    auto result = update_firmware(base_url + _firmware_name);

    if (result != HTTP_UPDATE_OK)
    {
      ESP_LOGI(TAG, "Update failed: %s\n", Updater.getLastErrorString().c_str());
      return;
    }

    ESP_LOGI(TAG, "Update successful. Restarting...\n");
    delay(1000);
    ESP.restart();
  }

  ESP_LOGI(TAG, "No updates found\n");
}

HTTPUpdateResult GitHubOTA::update_firmware(String url)
{
  const char *TAG = "update_firmware";
  ESP_LOGI(TAG, "Download URL: %s\n", url.c_str());

  auto result = Updater.update(_wifi_client, url);

  print_update_result(Updater, result, TAG);
  return result;
}
