#pragma once

// Device authentication is optional and deliberately separate from WiFi and
// backend configuration. The ignored local header must define:
//   constexpr const char *DEVICE_AUTH_TOKEN = "...";
#if __has_include("device_auth_local.h")
#include "device_auth_local.h"
#else
constexpr const char *DEVICE_AUTH_TOKEN = "";
#endif
