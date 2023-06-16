#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#define ESP_LOGE(tag, ...) Serial.printf("[ %s ]", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGW(tag, ...) Serial.printf("[ %s ]", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGI(tag, ...) Serial.printf("[ %s ]", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGD(tag, ...) Serial.printf("[ %s ]", tag); Serial.printf(__VA_ARGS__)
#define ESP_LOGV(tag, ...) Serial.printf("[ %s ]", tag); Serial.printf(__VA_ARGS__)
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <Update.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#endif

#include <ArduinoJson.h>
#include "semver_extensions.h"
#include "ota.h"

bool _fetch_url_via_redirect = false;
String _binary_filename = "firmware.bin";

WiFiClientSecure client;
semver_t current_version;

#ifdef ESP8266
ESP8266HTTPUpdate Updater;
#elif defined(ESP32)
HTTPUpdate Updater;
#endif

// Private function definitions
void update_firmware(String url);
void update_filesystem(String url);
void print_update_result(HTTPUpdateResult result, const char* TAG);
String get_redirect_location(String initial_url, WiFiClientSecure *client);


void init_ota(String version)
{
    init_ota(version, _binary_filename, false);
}

void init_ota(String version, String filename, bool fetch_url_via_redirect = false)
{
    ESP_LOGE("init_ota", "init_ota(version: %s, filename: %s, fetch_url_via_redirect: %d)\n",
        version.c_str(), filename.c_str(), fetch_url_via_redirect);

    Updater.rebootOnUpdate(false);
    _fetch_url_via_redirect = fetch_url_via_redirect;
    _binary_filename = filename;
    current_version = from_string(version.c_str());

#ifdef ESP32
    client.setCACert(github_certificate);
#elif defined(ESP8266)
    // BearSSL::X509List x509(github_certificate);
    // client.setTrustAnchors(&x509);
    client.setInsecure();
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

void handle_ota(String releaseUrl)
{
    const char *TAG = "handle_ota";

    String url = _fetch_url_via_redirect ?
        get_updated_firmware_url_via_redirect(releaseUrl, &client) :
        get_updated_firmware_url_via_api(releaseUrl, &client);

    ESP_LOGV(TAG, "get_updated_firmware_url returned: %s\n", url.c_str());

    if (url.length() > 0)
    {
        update_firmware(url);

        // url.replace("firmware", "filesystem");
        // update_filesystem(url);

        delay(1000);
        ESP.restart();
    }
}

void update_firmware(String url){
    const char *TAG = "update_firmware";

    ESP_LOGI(TAG, "Download URL: %s\n", url.c_str());

    auto result = Updater.update(client, url);
    print_update_result(result, TAG);
}

void update_filesystem(String url){
    const char *TAG = "update_filesystem";

    ESP_LOGI(TAG, "Download URL: %s\n", url.c_str());

#ifdef ESP8266
    auto result = Updater.updateFS(client, url);
#elif defined(ESP32)
    auto result = Updater.updateSpiffs(client, url);
#endif

    print_update_result(result, TAG);
}

void print_update_result(HTTPUpdateResult result, const char* TAG){
    switch (result)
    {
        case HTTP_UPDATE_FAILED:
            ESP_LOGI(TAG, "HTTP_UPDATE_FAILD Error (%d): %s\n", Updater.getLastError(), Updater.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            ESP_LOGI(TAG, "HTTP_UPDATE_NO_UPDATES\n");
            break;
        case HTTP_UPDATE_OK:
            ESP_LOGI(TAG, "HTTP_UPDATE_OK\n");
            break;
    }
}

String get_redirect_location(String initial_url, WiFiClientSecure *client)
{
    const char *TAG = "get_redirect_location";

    ESP_LOGV(TAG, "initial_url: %s\n", initial_url.c_str());
    String redirect_url = "";

    HTTPClient https;
    https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    client->setInsecure();
    bool mfln = client->probeMaxFragmentLength("github.com", 443, 1024);
    ESP_LOGI(TAG, "MFLN supported: %s\n", mfln ? "yes" : "no");
    if (mfln) { client->setBufferSizes(1024, 1024); }
    ESP_LOGV(TAG, "https.begin(%s)\n", initial_url.c_str());
    if (https.begin(*client, initial_url))
    {
        ESP_LOGV(TAG, "https.GET()\n");
        int httpCode = https.GET();
        char errortext[128];
        int errCode = client->getLastSSLError(errortext, 128);
        ESP_LOGV(TAG, "httpCode: %d, errorCode %d: %s\n", httpCode, errCode, errortext);
        if (httpCode != HTTP_CODE_FOUND)
        {
            ESP_LOGE(TAG, "[HTTPS] GET... failed, No redirect\n");
        }

        redirect_url = https.getLocation();
        https.end();
    }
    else
    {
        ESP_LOGE(TAG, "[HTTPS] Unable to connect\n");
    }

    ESP_LOGV(TAG, "returns: %s\n", redirect_url.c_str());
    return redirect_url;
}

String get_updated_firmware_url_via_redirect(String releaseUrl, WiFiClientSecure *client)
{
    const char *TAG = "get_updated_firmware_url_via_redirect";

    ESP_LOGV(TAG, "releaseUrl: %s\n", releaseUrl.c_str());
    String browser_download_url = "";

    auto location = get_redirect_location(releaseUrl, client);
    if (location.length() > 0)
    {
        ESP_LOGV(TAG, "location: %s\n", location.c_str());
        auto last_slash = location.lastIndexOf('/');
        auto semver_str = location.substring(last_slash + 1);

        auto _new_version = from_string(semver_str.c_str());
        ESP_LOGV(TAG, "Newest version: %s\n", render_to_string(&_new_version).c_str());
        ESP_LOGV(TAG, "Current version: %s\n", render_to_string(&current_version).c_str());
        ESP_LOGV(TAG, "Need update: %s\n", _new_version > current_version ? "yes" : "no");
        if (_new_version > current_version)
        {
            browser_download_url = String(location + "/" + _binary_filename);
            browser_download_url.replace("tag", "download");
        }
    }
    else
    {
        ESP_LOGE(TAG, "[HTTPS] No redirect url\n");
    }

    ESP_LOGV(TAG, "returns: %s\n", browser_download_url.c_str());
    return browser_download_url;
}

String get_updated_firmware_url_via_api(String releaseUrl, WiFiClientSecure *client)
{
    const char *TAG = "get_updated_firmware_url_via_api";

    ESP_LOGI(TAG, "releaseUrl: %s\n", releaseUrl.c_str());

    auto browser_download_url = "";

    HTTPClient https;
    https.useHTTP10(true);
    https.addHeader("Accept-Encoding", "identity");
    const char *headerKeys[] = {CONTENT_LENGTH_HEADER};
    https.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys)[0]);

    client->setInsecure();
    bool mfln = client->probeMaxFragmentLength("github.com", 443, 1024);
    ESP_LOGI(TAG, "MFLN supported: %s\n", mfln ? "yes" : "no");
    if (mfln) { client->setBufferSizes(1024, 1024); }
    if (https.begin(*client, releaseUrl))
    {
        ESP_LOGV(TAG, "https.GET()\n");
        int httpCode = https.GET();
        char errortext[128];
        int errCode = client->getLastSSLError(errortext, 128);
        ESP_LOGV(TAG, "httpCode: %d, errorCode %d: %s\n", httpCode, errCode, errortext);
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
            auto length = https.header(CONTENT_LENGTH_HEADER);
            auto content_length = length.toInt();

            DynamicJsonDocument doc(content_length);
            deserializeJson(doc, https.getStream());
            auto _new_version = from_string(doc["name"]);
            ESP_LOGV(TAG, "Newest version: %s\n", render_to_string(&_new_version).c_str());
            ESP_LOGV(TAG, "Current version: %s\n", render_to_string(&current_version).c_str());
            ESP_LOGV(TAG, "Need update: %s\n", _new_version > current_version ? "yes" : "no");

            if (_new_version > current_version)
            {
                JsonArray assets = doc["assets"].as<JsonArray>();
                for (JsonObject asset : assets) {
                    if(asset["name"] == _binary_filename){
                        browser_download_url = asset["browser_download_url"].as<const char *>();
                        break;
                    }
                }

                ESP_LOGE(TAG, "[HTTPS] No binary file found\n");
            }
            else
            {
                ESP_LOGI(TAG, "No updates found\n");
            }
        }

        https.end();
    }
    else
    {
        ESP_LOGI(TAG, "[HTTPS] Unable to connect\n");
    }

    return browser_download_url;
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

const char *github_certificate = R"(-----BEGIN CERTIFICATE-----
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
-----END CERTIFICATE-----)";