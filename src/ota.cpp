#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#elif defined(ESP32)
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#endif

#include <ArduinoJson.h>
#include "semver_extensions.h"
#include "ota.h"

WiFiClientSecure client;
Stream &serial = Serial;

semver_t current_version;

void init_ota(String version, Stream &serial)
{
    serial = serial;
    current_version = from_string(version.c_str());

#ifdef ESP8266
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);
#endif
}

void handle_ota(String releaseUrl)
{
#ifdef ESP32
    client.setCACert(github_certificate);
#elif defined(ESP8266)
    BearSSL::X509List x509(github_certificate);
    client.setTrustAnchors(&x509);
#endif
    auto url = get_updated_firmware_url(releaseUrl, &client);
    if (url.length() > 0)
    {
        serial.println("Download URL: " + url);

#ifdef ESP8266
        auto ret = ESPhttpUpdate.update(client, url);

        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
            serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            serial.println("HTTP_UPDATE_NO_UPDATES");
            break;
        case HTTP_UPDATE_OK:
            serial.println("HTTP_UPDATE_OK");
            break;
        }

#elif defined(ESP32)
        HttpsOTA.onHttpEvent(HttpEvent);
        HttpsOTA.begin(url.c_str(), github_certificate);

        HttpsOTAStatus_t otastatus = HttpsOTA.status();
        if (otastatus == HTTPS_OTA_SUCCESS)
        {
            serial.println("Firmware written successfully. To reboot device, call API ESP.restart() or PUSH restart button on device");
            ESP.restart();
        }
        else if (otastatus == HTTPS_OTA_FAIL)
        {
            serial.println("Firmware Upgrade Fail");
        }
#endif
    }
}

#ifdef DEBUG
String get_updated_firmware_url(String releaseUrl, WiFiClientSecure *client)
{
    String browser_download_url = "";

    HTTPClient https;
    const char *headerKeys[] = {"location"};
    https.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys)[0]);
    https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (https.begin(*client, releaseUrl))
    {
        int httpCode = https.GET();
        if (httpCode != HTTP_CODE_FOUND)
        {
            serial.println("[HTTPS] GET... failed, No redirect");
        }

        auto location = https.header("location");
        auto last_slash = location.lastIndexOf('/');
        auto semver_str = location.substring(last_slash + 1);

        auto _new_version = from_string(semver_str.c_str());
        if (_new_version > current_version)
        {
            browser_download_url = String(location + "/firmware.bin");
            browser_download_url.replace("tag", "download");
        }

        https.end();
    }
    else
    {
        serial.printf("[HTTPS] Unable to connect\n");
    }

    return browser_download_url;
}
#else
String get_updated_firmware_url(String releaseUrl, WiFiClientSecure *client)
{
    auto browser_download_url = "";

    HTTPClient https;
    https.useHTTP10(true);
    https.addHeader("Accept-Encoding", "identity");
    const char *headerKeys[] = {CONTENT_LENGTH_HEADER};
    https.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys)[0]);

    client->setInsecure();
    if (https.begin(*client, releaseUrl))
    {
        int httpCode = https.GET();
        if (httpCode < 0 )
        {
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        else if (httpCode >= 400)
        {
            Serial.printf("[HTTPS] GET... failed, HTTP Status code: %d\n", httpCode);
        }
        else if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            auto length = https.header(CONTENT_LENGTH_HEADER);
            auto content_length = length.toInt();

            DynamicJsonDocument doc(content_length);
            deserializeJson(doc, https.getStream());
            auto _new_version = from_string(doc["name"]);

            if (_new_version > current_version)
            {
                browser_download_url = doc["assets"][0]["browser_download_url"];
            }
            else
            {
                serial.println("No updates found");
            }
        }

        https.end();
    }
    else
    {
        Serial.printf("[HTTPS] Unable to connect\n");
    }

    return browser_download_url;
}
#endif

int totalBytes = -1;
int currentyReceiced = 0;

#ifdef ESP32
void HttpEvent(HttpEvent_t *event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ERROR:
        update_error(-1);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        serial.println("Http Event On Connected");
        break;
    case HTTP_EVENT_HEADER_SENT:
        serial.println("Http Event Header Sent");
        break;
    case HTTP_EVENT_ON_HEADER:
        serial.printf("Http Event On Header, key=%s, value=%s\n", event->header_key, event->header_value);
        if (strcmp(event->header_key, "Content-Length") == 0)
        {
            totalBytes = atoi(event->header_value);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        currentyReceiced += event->data_len;
        update_progress(currentyReceiced, totalBytes);
        break;
    case HTTP_EVENT_ON_FINISH:
        update_finished();
        break;
    case HTTP_EVENT_DISCONNECTED:
        serial.println("\nHttp Event Disconnected");
        break;
    }
}
#endif

void update_started()
{
    serial.println("HTTP update process started");
}

void update_finished()
{
    serial.println("HTTP update process finished");
}

void update_progress(int currentyReceiced, int totalBytes)
{
    serial.printf("\rData received, Progress: %.2f %%", 100.0 * currentyReceiced / totalBytes);
}

void update_error(int err)
{
    serial.printf("HTTP update fatal error code %d\n", err);
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