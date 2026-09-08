#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <vector>

struct Theme {
  const char *name;
  uint16_t bg;
  uint16_t primary;
  uint16_t accent;
};

static const Theme THEMES[] = {
  {"Cyber",  0x0000, 0xFFFF, 0x07FF},
  {"Amber",  0x0000, 0xFD20, 0xFBE0},
  {"Mint",   0x0000, 0xFFFF, 0x07E6},
  {"Rose",   0x0000, 0xFFFF, 0xF81F},
  {"Mono",   0x0000, 0xFFFF, 0x8410},
  {"Sunset", 0x0000, 0xFFFF, 0xFB20},
};
static const int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);

struct Settings {
  String wifiSsid = "";
  String wifiPassword = "";
  int themeIndex = 0;
  int brightnessPct = 80;
  int volumePct = 60;
};

extern Settings gSettings;

void settingsLoad();
void settingsSave();
void settingsApplyDisplayAndAudio();
bool settingsHasWifi();
void settingsRunMenu();
void settingsRunWifiSetup();