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
#include "semver.h"
#include "semver_extensions.h"
#include "GitHubFsOTA.h"
#include "common.h"

GitHubFsOTA::GitHubFsOTA(
    String version,
    String release_url,
    String filesystem_name,
    bool fetch_url_via_redirect)
{
  ESP_LOGV("GitHubFsOTA", "GitHubFsOTA(version: %s, filesystem_name: %s, fetch_url_via_redirect: %d)\n",
           version.c_str(), filesystem_name.c_str(), fetch_url_via_redirect);

  _version = semver_from_string(version.c_str());
  _release_url = release_url;
  _filesystem_name = filesystem_name;
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

void GitHubFsOTA::handle()
{
  const char *TAG = "handle";
  
  auto base_url = get_base_url(_fetch_url_via_redirect, _wifi_client, _release_url);

  auto last_slash = base_url.lastIndexOf('/', base_url.length() - 2);
  auto semver_str = base_url.substring(last_slash + 1);
  auto newest_version = semver_from_string(semver_str.c_str());

  if (newest_version > _version)
  {
    auto result = update_filesystem(base_url + _filesystem_name);

    if (result != HTTP_UPDATE_OK)
    {
      ESP_LOGI(TAG, "FS update failed: %s\n", Updater.getLastErrorString().c_str());
      return;
    }

    ESP_LOGI(TAG, "FS update successful.\n");
    // ESP.restart();
    return;
  }

  ESP_LOGI(TAG, "No updates found\n");
}

semver_t GitHubFsOTA::get_newest_version()
{
  return common_get_newest_version(_fetch_url_via_redirect, _wifi_client, _release_url);
}

void GitHubFsOTA::update_filesystem()
{
  handle();
}

HTTPUpdateResult GitHubFsOTA::update_filesystem(String url)
{
  const char *TAG = "update_filesystem";
  ESP_LOGI(TAG, "Download URL: %s\n", url.c_str());

#ifdef ESP8266
  auto result = Updater.updateFS(_wifi_client, url);
#elif defined(ESP32)
  auto result = Updater.updateSpiffs(_wifi_client, url);
#endif
  print_update_result(Updater, result, TAG);
  return result;
}