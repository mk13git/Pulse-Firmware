#pragma once

// --- Fill these in before flashing ---
// WiFi is no longer set here — on first boot with nothing saved, the device
// walks you through picking a network and typing a password on-device, and
// remembers it across reboots. Press 'S' any time to change it later.

// Your deployed Pulse origin, no trailing slash, e.g. https://pulse-xyz.vercel.app
#define PULSE_BASE_URL "https://your-pulse-deploy.vercel.app"

// Station bearer token — issued from the "Stations" page in Pulse
// (Pair a station -> kind "Cardputer" -> copy the pulse_... token shown once)
#define STATION_TOKEN "pulse_xxxxxxxxxxxxxxxxxxxxxxxxxxxx"

#define POLL_INTERVAL_MS 5000
#define MAX_RECORD_SECONDS 20
#define MIC_SAMPLE_RATE 16000