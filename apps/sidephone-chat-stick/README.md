# Sidephone Chat Stick

Native Android client for the chat-stick `/ws` device protocol.

It supports:

- push-to-talk 16 kHz PCM microphone capture
- 24 kHz PCM assistant audio playback
- a Chat Stick-style 240x135 monochrome display
- hardware controls only: `! Q W` key = push-to-talk; volume keys control volume
- basic device tool responses for volume, brightness, text display, sound, status, and timers

## Build

```bash
./build.sh
```

The script uses the Android SDK command-line tools directly. It does not require Gradle.

## Install

```bash
adb install -r build/chat-stick-sidephone-debug.apk
adb shell pm grant com.tldraw.chatstick android.permission.RECORD_AUDIO
```

The installed app tries `http://127.0.0.1:8788` first to avoid conflicts with other local Wrangler sessions, then falls back to the production Worker at `https://m5-live.tldraw.workers.dev`. Start the local Worker on that port with:

```bash
cd ../../server
npm run dev -- --port 8788 --ip 0.0.0.0
adb reverse tcp:8788 tcp:8788
```

The app auto-connects on launch. Hold `! Q W`, D-pad up, or play/pause to talk and release to send. Use `1 E R`, D-pad down, or D-pad right as Button B for paging/dismiss. Volume keys are left to Android for media volume.
