// Pulse pager firmware — M5Stack Cardputer ADV
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wav.h"
#include "settings.h"

M5Canvas canvas(&M5Cardputer.Display);

String lastSeenSignalId = "";
unsigned long lastPollAt = 0;
String statusLine = "booting...";
String lastTranscript = "";

bool recording = false;
int16_t *recordBuffer = nullptr;
size_t recordCapacitySamples = 0;
size_t recordedSamples = 0;
unsigned long recordStartedAt = 0;

void drawStatus(const String &title, const String &body, uint16_t color) {
  const Theme &t = THEMES[gSettings.themeIndex];
  canvas.fillScreen(t.bg);
  canvas.setTextColor(t.primary);
  canvas.setTextSize(1);
  canvas.setCursor(4, 4);
  canvas.print(WiFi.status() == WL_CONNECTED ? "wifi ok" : "wifi ...");
  canvas.setCursor(180, 4);
  canvas.print("[S]ettings");

  canvas.setTextColor(color);
  canvas.setTextSize(2);
  canvas.setCursor(4, 20);
  canvas.print(title);

  canvas.setTextColor(t.primary);
  canvas.setTextSize(1);
  canvas.setCursor(4, 48);
  int lineLen = 34;
  int i = 0;
  while (i < (int)body.length() && canvas.getCursorY() < 130) {
    canvas.println(body.substring(i, i + lineLen));
    i += lineLen;
  }
  canvas.pushSprite(0, 0);
}

bool httpGetJson(const String &path, JsonDocument &doc) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(PULSE_BASE_URL) + path;
  if (!http.begin(client, url)) return false;
  http.addHeader("Authorization", String("Bearer ") + STATION_TOKEN);
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    DeserializationError err = deserializeJson(doc, http.getStream());
    ok = !err;
  }
  http.end();
  return ok;
}

bool uploadVoice(const int16_t *samples, size_t sampleCount, JsonDocument &responseDoc) {
  uint32_t dataBytes = sampleCount * 2;
  uint32_t totalWavBytes = WAV_HEADER_SIZE + dataBytes;
  const char *boundary = "----PulseCardputerBoundary";
  String head = String("--") + boundary + "\r\n" +
                "Content-Disposition: form-data; name=\"audio\"; filename=\"voice.wav\"\r\n" +
                "Content-Type: audio/wav\r\n\r\n";
  String tail = String("\r\n--") + boundary + "--\r\n";
  uint32_t contentLength = head.length() + totalWavBytes + tail.length();

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(PULSE_BASE_URL) + "/api/gateway/voice";
  if (!http.begin(client, url)) return false;
  http.addHeader("Authorization", String("Bearer ") + STATION_TOKEN);
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);

  uint8_t *wav = (uint8_t *)ps_malloc(totalWavBytes);
  if (!wav) { http.end(); return false; }
  writeWavHeader(wav, dataBytes, MIC_SAMPLE_RATE);
  memcpy(wav + WAV_HEADER_SIZE, samples, dataBytes);

  uint8_t *full = (uint8_t *)ps_malloc(contentLength);
  if (!full) { free(wav); http.end(); return false; }
  memcpy(full, head.c_str(), head.length());
  memcpy(full + head.length(), wav, totalWavBytes);
  memcpy(full + head.length() + totalWavBytes, tail.c_str(), tail.length());
  free(wav);

  int code = http.POST(full, contentLength);
  free(full);

  bool ok = false;
  if (code == 200) {
    DeserializationError err = deserializeJson(responseDoc, http.getStream());
    ok = !err;
  } else {
    statusLine = "voice upload failed: " + String(code);
  }
  http.end();
  return ok;
}

void pollSignals() {
  JsonDocument doc;
  if (!httpGetJson("/api/gateway/signals?limit=5", doc)) return;
  JsonArray signals = doc["signals"].as<JsonArray>();
  if (signals.size() == 0) return;
  JsonObject latest = signals[0];
  String id = latest["id"].as<String>();
  if (id == lastSeenSignalId) return;
  lastSeenSignalId = id;
  String kind = latest["kind"].as<String>();
  String title = latest["title"].as<String>();
  String body = latest["body"].as<String>();
  int freq = kind == "approval" ? 1800 : kind == "handoff" ? 900 : kind == "voice_task" ? 1400 : 1200;
  M5Cardputer.Speaker.tone(freq, 120);
  uint16_t color = kind == "approval" ? TFT_YELLOW : kind == "handoff" ? TFT_RED : kind == "complete" ? TFT_GREEN : TFT_CYAN;
  drawStatus(title, body, color);
}

void startRecording() {
  if (!psramFound()) { statusLine = "no PSRAM found"; return; }
  recordCapacitySamples = MIC_SAMPLE_RATE * MAX_RECORD_SECONDS;
  recordBuffer = (int16_t *)ps_malloc(recordCapacitySamples * sizeof(int16_t));
  if (!recordBuffer) { statusLine = "record alloc failed"; return; }
  recordedSamples = 0;
  recording = true;
  recordStartedAt = millis();
  drawStatus("Listening...", "release ENTER to send", TFT_MAGENTA);
  M5Cardputer.Mic.begin();
}

void stopRecordingAndSend() {
  recording = false;
  M5Cardputer.Mic.end();
  if (recordedSamples < MIC_SAMPLE_RATE / 2) {
    drawStatus("Too short", "hold ENTER longer next time", TFT_ORANGE);
    free(recordBuffer);
    recordBuffer = nullptr;
    return;
  }
  drawStatus("Sending...", "transcribing on the server", TFT_CYAN);
  JsonDocument resp;
  bool ok = uploadVoice(recordBuffer, recordedSamples, resp);
  free(recordBuffer);
  recordBuffer = nullptr;
  if (ok && resp["ok"] == true) {
    lastTranscript = resp["transcript"].as<String>();
    drawStatus("Queued", lastTranscript, TFT_GREEN);
    M5Cardputer.Speaker.tone(1600, 100);
  } else {
    String err = resp["error"] | "upload failed";
    drawStatus("Voice failed", err, TFT_RED);
    M5Cardputer.Speaker.tone(400, 200);
  }
}

void connectWiFi() {
  if (!settingsHasWifi()) {
    settingsRunWifiSetup();
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(gSettings.wifiSsid.c_str(), gSettings.wifiPassword.c_str());
  drawStatus("Connecting", gSettings.wifiSsid, TFT_CYAN);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    drawStatus("Pulse online", WiFi.localIP().toString(), TFT_GREEN);
  } else {
    drawStatus("WiFi failed", "press S for settings", TFT_RED);
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  canvas.setTextWrap(false);

  settingsLoad();
  settingsApplyDisplayAndAudio();
  connectWiFi();
}

void loop() {
  M5Cardputer.update();

  bool enterHeld = M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER);

  if (!recording && M5Cardputer.Keyboard.isKeyPressed('s')) {
    settingsRunMenu();
    settingsApplyDisplayAndAudio();
    if (WiFi.status() != WL_CONNECTED) connectWiFi();
    return;
  }

  if (enterHeld && !recording) {
    startRecording();
  } else if (!enterHeld && recording) {
    stopRecordingAndSend();
  }

  if (recording) {
    size_t room = recordCapacitySamples - recordedSamples;
    if (room > 0) {
      size_t got = M5Cardputer.Mic.record(recordBuffer + recordedSamples, room, MIC_SAMPLE_RATE);
      recordedSamples += got;
    }
    if (millis() - recordStartedAt > (unsigned long)MAX_RECORD_SECONDS * 1000) {
      stopRecordingAndSend();
    }
    return;
  }

  unsigned long now = millis();
  if (now - lastPollAt > POLL_INTERVAL_MS && WiFi.status() == WL_CONNECTED) {
    lastPollAt = now;
    pollSignals();
  }
}