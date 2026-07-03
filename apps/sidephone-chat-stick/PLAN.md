# Sidephone Chat Stick — Working Plan & State

Living document for the native Android Sidephone client. Captures current
behavior, the server-side changes it depends on, and how to build/verify.

## Device

- Sidephone connected over ADB, device id `SP01GE260600545`.
- Android package: `com.tldraw.chatstick`.
- Native Android client in `apps/sidephone-chat-stick`, built by
  `./apps/sidephone-chat-stick/build.sh` **without Gradle** (uses the Android
  SDK command-line tools directly).

## Current app behavior

- Installed on the Sidephone.
- Connects to local `http://127.0.0.1:8788` first, then falls back to prod
  `https://m5-live.tldraw.workers.dev`.
- Push-to-talk **Button A** keys:
  - `! Q W` / `KEYCODE_Q`
  - D-pad up
  - play/pause
  - headset hook
- **Button B** / paging-dismiss keys:
  - `1 E R` / `KEYCODE_E`
  - D-pad down
  - D-pad right
- `Q` was tricky: Sidephone JakeType consumes keyboard keys, so the app uses
  `dispatchKeyEvent` and `onKeyPreIme` in `ChatStickDisplayView`.
  **Do not remove that.**
- Volume keys are left for Android media volume. The app does not auto-raise
  playback volume. The explicit `set_volume` tool still works.
- Full 480x640 screen, black background, white monospace 16px text, padded from
  top/left for rounded corners.
- Images request 360x360 and render centered with aspect ratio preserved.
- `play_melody` is implemented in Android via PCM sine-wave rendering from notes
  like `"C4:200 E4:200 G4:400"`; `play_sound` supports
  `beep`/`success`/`error`/`alert`/`fanfare`.
- Audio playback is interrupted immediately when Button A starts recording.
- No separate "Listening…" screen; recording dims existing content like Chat
  Stick conventions.

## Server-side changes already made (`server/src/live-session.ts`)

Changed to better handle Sidephone turns:
- Typed text support via `clientContent`.
- `queuedTextInputs`.
- `deviceRecording` guard so late binary chunks after stop are ignored.
- Quiet turns are no longer rejected as "silent"; only too-short turns are
  ignored.

## Important files

- `apps/sidephone-chat-stick/src/main/java/com/tldraw/chatstick/MainActivity.java`
- `apps/sidephone-chat-stick/build.sh`
- `apps/sidephone-chat-stick/README.md`
- `server/src/live-session.ts`

## Build / install / launch

```bash
./apps/sidephone-chat-stick/build.sh
adb install -r apps/sidephone-chat-stick/build/chat-stick-sidephone-debug.apk
adb shell pm grant com.tldraw.chatstick android.permission.RECORD_AUDIO
adb reverse tcp:8788 tcp:8788
adb shell svc power stayon true
adb shell input keyevent 224
adb shell am force-stop com.tldraw.chatstick
adb shell am start -n com.tldraw.chatstick/.MainActivity
```

## Verification

```bash
adb devices
adb shell pm path com.tldraw.chatstick
adb shell dumpsys input | sed -n '/Input Dispatcher State:/,/Input Reader State:/p' \
  | rg -i "FocusedWindows|FocusedApplications|com.tldraw.chatstick"
git diff --check
(cd server && npm exec tsc -- --noEmit)
```

## Repo state caveat

`apps/sidephone-chat-stick` is currently **untracked** in git. `.gitignore` and
`server/src/live-session.ts` are modified. Do not revert unrelated changes.

## Working conventions

- Use existing patterns in `MainActivity.java`. Keep changes scoped.
- Rebuild and reinstall on the Sidephone after Android changes.
- If checking prior context/docs, use the local qmd MCP search tools when
  available.
