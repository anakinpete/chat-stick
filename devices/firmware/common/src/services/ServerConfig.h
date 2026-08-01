#pragma once

#include "Config.h"

#if __has_include("server_config_local.h")
#include "server_config_local.h"
#else
// No deployment-specific backend is compiled into the shared firmware.
// A valid endpoint saved through the setup portal becomes the primary endpoint.
constexpr const char *DEFAULT_BACKEND_HOST = "";
constexpr int DEFAULT_BACKEND_PORT = 0;

// Keep the endpoint abstraction available without inventing a fallback host.
constexpr const ServerEndpoint *SERVER_ENDPOINTS = nullptr;
constexpr int SERVER_ENDPOINT_COUNT = 0;
#endif
