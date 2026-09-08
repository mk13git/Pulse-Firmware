# Pulse Cardputer firmware — v1

WiFi connect, polls `/api/gateway/signals` for new pings (beep + on-screen
display), and press-and-hold voice tasks (ENTER to record, release to
upload to `/api/gateway/voice`, transcript shown back on screen).

Not yet done: handoff/reroute UI on-device, the Pip mascot, marking
signals read from the device.

Set `OPENAI_API_KEY` on the Vercel project or `/api/gateway/voice` will 502.

Written against documented M5Unified/M5Cardputer APIs but not yet run on
real hardware. Most likely spots to need a fix on first flash: the
`M5Cardputer.Mic.begin()/.record()` signature (shifted across library
versions), the `board =` line in platformio.ini, and the ENTER-key mapping.