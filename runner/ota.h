// ============================================================
//  ota.h  -  GitHub auto-updater for the receiver ESP32
//  Runs in the background on core 0. Every 10 s it checks
//  GitHub for a newer build and installs it.
//  Wi-Fi + repo settings are read from flash (saved once by
//  the TargetSetup sketch), so they never go on GitHub.
// ============================================================
#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_system.h>
#include "build_info.h"   // defines BUILD_VERSION

#ifndef OTA_CHECK_MS
#define OTA_CHECK_MS 10000
#endif

static String ota_ssid, ota_pass, ota_repo;
static bool   ota_safeMode = false;

// Counts crashes that happen right after boot. If the uploaded
// code crashes 3 times in a row, we stop running it ("safe mode")
// so the updater can still receive a fixed version.
RTC_NOINIT_ATTR static uint32_t ota_crashCount;
RTC_NOINIT_ATTR static uint32_t ota_magic;

// ---- Save settings (used once by TargetSetup) ----
inline void otaSaveSettings(const char* ssid, const char* pass, const char* repo) {
  Preferences p;
  p.begin("ota", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.putString("repo", repo);
  p.end();
}

inline bool otaSafeMode() { return ota_safeMode; }

static String otaBaseUrl() {
  return "https://github.com/" + ota_repo + "/releases/download/latest-build/";
}

static String otaGetText(const String& url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return "";
  String out;
  if (http.GET() == 200) out = http.getString();
  http.end();
  out.trim();
  return out;
}

static bool otaInstall() {
  Serial.println("[OTA] Downloading new program...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(20000);
  if (!http.begin(client, otaBaseUrl() + "firmware.bin")) return false;

  bool ok = false;
  if (http.GET() == 200) {
    int len = http.getSize();
    if (Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) {
      size_t written = Update.writeStream(*http.getStreamPtr());
      if (Update.end(true) && (len <= 0 || (int)written == len)) ok = true;
    }
    if (!ok) Serial.printf("[OTA] Update failed: %s\n", Update.errorString());
  }
  http.end();
  return ok;
}

static void otaTask(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ota_ssid.c_str(), ota_pass.c_str());
  unsigned long bootMs = millis();

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      static unsigned long lastTry = 0;
      if (millis() - lastTry > 15000) {
        lastTry = millis();
        WiFi.disconnect();
        WiFi.begin(ota_ssid.c_str(), ota_pass.c_str());
      }
    } else {
      String latest = otaGetText(otaBaseUrl() + "version.txt");
      if (latest.length() >= 7 && latest != BUILD_VERSION) {
        Serial.printf("[OTA] New version %s (running %s)\n", latest.c_str(), BUILD_VERSION);
        if (otaInstall()) {
          Serial.println("[OTA] Installed! Restarting...");
          ota_crashCount = 0;
          delay(300);
          ESP.restart();
        }
      }
    }
    // Ran 20 s without crashing = the program is stable
    if (millis() - bootMs > 20000) ota_crashCount = 0;
    vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_MS));
  }
}

// ---- Call first thing in setup() ----
inline void otaStart() {
  if (!Serial) Serial.begin(115200);

  // Crash counter (survives resets, cleared on power-up)
  esp_reset_reason_t why = esp_reset_reason();
  if (ota_magic != 0xC0DEC0DE || why == ESP_RST_POWERON) { ota_magic = 0xC0DEC0DE; ota_crashCount = 0; }
  if (why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT || why == ESP_RST_WDT)
    ota_crashCount++;
  ota_safeMode = (ota_crashCount >= 3);

  Preferences p;
  p.begin("ota", true);
  ota_ssid = p.getString("ssid", "");
  ota_pass = p.getString("pass", "");
  ota_repo = p.getString("repo", "");
  p.end();

  Serial.printf("\n[OTA] Running build %s\n", BUILD_VERSION);
  if (ota_safeMode) Serial.println("[OTA] Your code crashed 3 times - SAFE MODE (waiting for a fix)");
  if (ota_ssid.isEmpty() || ota_repo.isEmpty()) {
    Serial.println("[OTA] No Wi-Fi/repo saved. Flash TargetSetup first!");
    return;
  }
  xTaskCreatePinnedToCore(otaTask, "ota", 12288, nullptr, 1, nullptr, 0);
}
