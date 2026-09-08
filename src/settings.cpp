#include "settings.h"
#include <M5GFX.h>

extern M5Canvas canvas;

Settings gSettings;
static Preferences prefs;

void settingsLoad() {
  prefs.begin("pulse", false);
  gSettings.wifiSsid = prefs.getString("ssid", "");
  gSettings.wifiPassword = prefs.getString("pass", "");
  gSettings.themeIndex = prefs.getInt("theme", 0);
  gSettings.brightnessPct = prefs.getInt("bright", 80);
  gSettings.volumePct = prefs.getInt("vol", 60);
  prefs.end();
  if (gSettings.themeIndex < 0 || gSettings.themeIndex >= THEME_COUNT) gSettings.themeIndex = 0;
  if (gSettings.brightnessPct < 10) gSettings.brightnessPct = 10;
  if (gSettings.volumePct < 0) gSettings.volumePct = 0;
}

void settingsSave() {
  prefs.begin("pulse", false);
  prefs.putString("ssid", gSettings.wifiSsid);
  prefs.putString("pass", gSettings.wifiPassword);
  prefs.putInt("theme", gSettings.themeIndex);
  prefs.putInt("bright", gSettings.brightnessPct);
  prefs.putInt("vol", gSettings.volumePct);
  prefs.end();
}

bool settingsHasWifi() { return gSettings.wifiSsid.length() > 0; }

void settingsApplyDisplayAndAudio() {
  M5Cardputer.Display.setBrightness((gSettings.brightnessPct * 255) / 100);
  M5Cardputer.Speaker.setVolume((gSettings.volumePct * 255) / 100);
}

struct KeyEvent {
  bool left = false, right = false, up = false, down = false;
  bool enter = false, back = false;
  String typed = "";
};

KeyEvent pollKeys() {
  KeyEvent ev;
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return ev;
  auto state = M5Cardputer.Keyboard.keysState();
  ev.enter = state.enter;
  ev.back = state.del;
  for (char c : state.word) {
    if (c == ',') ev.left = true;
    else if (c == '/') ev.right = true;
    else if (c == ';') ev.up = true;
    else if (c == '.') ev.down = true;
    else ev.typed += c;
  }
  return ev;
}

void drawHeader(const char *title) {
  const Theme &t = THEMES[gSettings.themeIndex];
  canvas.fillScreen(t.bg);
  canvas.setTextColor(t.accent);
  canvas.setTextSize(2);
  canvas.setCursor(4, 4);
  canvas.print(title);
  canvas.setTextColor(t.primary);
  canvas.setTextSize(1);
}

String textInputScreen(const char *prompt, bool mask) {
  String value = "";
  while (true) {
    drawHeader(prompt);
    canvas.setCursor(4, 30);
    canvas.setTextSize(1);
    String shown = "";
    for (size_t i = 0; i < value.length(); i++) shown += mask ? '*' : value[i];
    canvas.println(shown + "_");
    canvas.setCursor(4, 110);
    canvas.print("ENTER=ok  DEL=back");
    canvas.pushSprite(0, 0);

    KeyEvent ev = pollKeys();
    if (ev.enter) return value;
    if (ev.back) {
      if (value.length() > 0) value.remove(value.length() - 1);
      else return "";
    }
    if (ev.typed.length() > 0) value += ev.typed;
    delay(30);
  }
}

void settingsRunWifiSetup() {
  drawHeader("Scanning WiFi...");
  canvas.pushSprite(0, 0);
  int n = WiFi.scanNetworks();

  std::vector<String> names;
  for (int i = 0; i < n && i < 20; i++) names.push_back(WiFi.SSID(i));
  names.push_back("[ Enter manually ]");

  int sel = 0;
  while (true) {
    drawHeader("Pick WiFi");
    for (size_t i = 0; i < names.size() && i < 8; i++) {
      canvas.setCursor(4, 30 + i * 12);
      canvas.setTextColor((int)i == sel ? THEMES[gSettings.themeIndex].accent : THEMES[gSettings.themeIndex].primary);
      canvas.println(names[i]);
    }
    canvas.pushSprite(0, 0);

    KeyEvent ev = pollKeys();
    if (ev.up && sel > 0) sel--;
    if (ev.down && sel < (int)names.size() - 1) sel++;
    if (ev.back) return;
    if (ev.enter) break;
    delay(30);
  }

  String ssid = (names[sel] == "[ Enter manually ]") ? textInputScreen("SSID:", false) : names[sel];
  if (ssid.length() == 0) return;
  String pass = textInputScreen("Password:", true);

  gSettings.wifiSsid = ssid;
  gSettings.wifiPassword = pass;
  settingsSave();

  drawHeader("Saved");
  canvas.setCursor(4, 30);
  canvas.println(ssid);
  canvas.println("Reconnecting...");
  canvas.pushSprite(0, 0);
  delay(800);
}

void themePickerScreen() {
  while (true) {
    const Theme &t = THEMES[gSettings.themeIndex];
    drawHeader("Theme");
    canvas.setCursor(4, 40);
    canvas.setTextSize(2);
    canvas.setTextColor(t.accent);
    canvas.println(t.name);
    canvas.setTextSize(1);
    canvas.setTextColor(t.primary);
    canvas.setCursor(4, 70);
    canvas.printf("%d / %d", gSettings.themeIndex + 1, THEME_COUNT);
    canvas.setCursor(4, 110);
    canvas.print(",/= change  ENTER=ok");
    canvas.pushSprite(0, 0);

    KeyEvent ev = pollKeys();
    if (ev.left) gSettings.themeIndex = (gSettings.themeIndex - 1 + THEME_COUNT) % THEME_COUNT;
    if (ev.right) gSettings.themeIndex = (gSettings.themeIndex + 1) % THEME_COUNT;
    if (ev.enter || ev.back) { settingsSave(); return; }
    delay(30);
  }
}

int percentPickerScreen(const char *title, int startPct, int minPct) {
  int pct = startPct;
  while (true) {
    const Theme &t = THEMES[gSettings.themeIndex];
    drawHeader(title);
    canvas.setTextSize(2);
    canvas.setCursor(4, 40);
    canvas.setTextColor(t.accent);
    canvas.printf("%d%%", pct);
    canvas.setTextSize(1);
    canvas.setTextColor(t.primary);

    int barW = 220, barX = 4, barY = 70;
    canvas.drawRect(barX, barY, barW, 10, t.primary);
    canvas.fillRect(barX, barY, (barW * pct) / 100, 10, t.accent);

    canvas.setCursor(4, 110);
    canvas.print(",/= adjust  ENTER=ok");
    canvas.pushSprite(0, 0);

    KeyEvent ev = pollKeys();
    if (ev.left) pct = max(minPct, pct - 10);
    if (ev.right) pct = min(100, pct + 10);
    if (ev.enter || ev.back) return pct;
    delay(30);
  }
}

void settingsRunMenu() {
  const char *items[] = {"WiFi", "Theme", "Brightness", "Volume", "Back"};
  int sel = 0;
  while (true) {
    drawHeader("Settings");
    for (int i = 0; i < 5; i++) {
      canvas.setCursor(4, 30 + i * 14);
      canvas.setTextColor(i == sel ? THEMES[gSettings.themeIndex].accent : THEMES[gSettings.themeIndex].primary);
      canvas.println(items[i]);
    }
    canvas.pushSprite(0, 0);

    KeyEvent ev = pollKeys();
    if (ev.up) sel = (sel - 1 + 5) % 5;
    if (ev.down) sel = (sel + 1) % 5;
    if (ev.back) return;

    if (ev.enter) {
      switch (sel) {
        case 0: settingsRunWifiSetup(); break;
        case 1: themePickerScreen(); break;
        case 2:
          gSettings.brightnessPct = percentPickerScreen("Brightness", gSettings.brightnessPct, 10);
          settingsSave();
          settingsApplyDisplayAndAudio();
          break;
        case 3:
          gSettings.volumePct = percentPickerScreen("Volume", gSettings.volumePct, 0);
          settingsSave();
          settingsApplyDisplayAndAudio();
          break;
        case 4: return;
      }
    }
    delay(30);
  }
}