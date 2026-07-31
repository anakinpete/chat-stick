# Browser Setup Portal Specification

## Purpose

Allow the user to configure Wi-Fi and backend connectivity from a phone or computer without editing source files or reflashing firmware.

## MVP entry conditions

Setup mode starts when:

- no saved Wi-Fi settings exist;
- stored settings fail validation;
- the user explicitly enters setup mode at boot once a gesture is finalized;
- connection repeatedly fails long enough to indicate that saved credentials are probably unusable.

A brief, ordinary network outage should not immediately erase settings or force setup mode.

## Access point

Suggested MVP SSID:

```text
M5-Companion-Setup
```

Setup page:

```text
http://192.168.4.1
```

A captive portal redirect is desirable but is not required for the first working version.

## Required form fields

- Wi-Fi SSID
- Wi-Fi password
- Backend address
- Backend port

An SSID scan/dropdown is useful but optional for the first implementation. Manual SSID entry is acceptable for the MVP.

## Later form fields

- Device name
- Theme
- Timezone
- Display brightness
- 12/24-hour clock
- Metric/imperial units
- AI provider or backend mode

These later fields must not delay the first network-configuration milestone.

## Save and validation behavior

1. Validate that required text fields are present.
2. Validate the backend port as a usable integer.
3. Do not echo the submitted password back into the page or serial output.
4. Store the values using the shared settings abstraction and ESP32 Preferences.
5. Show a clear success or failure state on the browser and device display.
6. Restart after a successful save.
7. Attempt normal connection using the newly saved settings.
8. If connection fails, preserve the values and offer a recoverable route back to setup.

## Device display during setup

Minimum display:

```text
SETUP MODE

Wi-Fi: M5-Companion-Setup
Open: 192.168.4.1
```

Also provide states for:

- waiting for browser connection;
- saving;
- validation error;
- saved successfully;
- restarting;
- connection failed.

## Security requirements

- Never log the Wi-Fi password.
- Never include real credentials in committed source or documentation.
- Use an HTML password input.
- Do not prefill the stored password into the form.
- Keep setup mode active only when needed.
- Do not expose a settings endpoint during normal operation unless deliberately designed and protected.

An AP password and stronger protection may be added later. The MVP portal is a local provisioning mechanism, not an internet-facing administration interface.

## Configuration storage

The portal must use the same settings abstraction as normal startup. Avoid one code path reading `credentials.h` while another reads Preferences indefinitely.

Development-time fallback constants may remain temporarily, but normal user configuration should prefer saved settings.

## Acceptance criteria

- A fresh device can be configured from a phone browser.
- Saved settings survive power loss and restart.
- Changing networks does not require recompilation.
- Backend address and port can be changed from the portal.
- The password is absent from logs and UI after submission.
- Invalid input produces a recoverable error.
- Existing display and backend behavior remain intact outside setup mode.
