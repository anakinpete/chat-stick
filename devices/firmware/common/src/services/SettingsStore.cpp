#include "SettingsStore.h"

#include "Config.h"
#include "WiFiService.h"
#include "credentials.h"
#include "state/ThemeManager.h"

namespace {
bool isValidPort(int port) { return port >= 1 && port <= 65535; }

bool writeVerifiedString(Preferences &prefs, const char *key,
                         const String &value) {
  if (value.isEmpty() && prefs.isKey(key) && !prefs.remove(key)) {
    return false;
  }

  const size_t written = prefs.putString(key, value);
  if ((!value.isEmpty() && written != value.length()) || !prefs.isKey(key)) {
    return false;
  }
  return prefs.getString(key, "") == value;
}

void restoreStoredString(Preferences &prefs, const char *key, bool existed,
                         const String &value) {
  if (existed) {
    writeVerifiedString(prefs, key, value);
  } else if (prefs.isKey(key)) {
    prefs.remove(key);
  }
}
} // namespace

void SettingsStore::init() {
  _brightness = DEFAULT_BRIGHTNESS;
  _volume = DEFAULT_VOLUME;
  _chatId = "";
  _useExternalSpeaker = false;
  _externalSpeakerGain = kDefaultExternalGain;
  _voice = kDefaultVoice;
  _serverEndpointIndex = 0;
  _wifiSsid = WIFI_NETWORK_COUNT > 0 ? WIFI_NETWORKS[0].ssid : "";
  _wifiPassword = WIFI_NETWORK_COUNT > 0 ? WIFI_NETWORKS[0].password : "";
  _backendHost = DEVELOPMENT_SERVER_ADDRESS;
  _backendPort = DEVELOPMENT_SERVER_PORT;
  _activeTheme = ThemeManager::identifier(ThemeId::Plain);
  _hasSavedWifi = false;
  _hasSavedBackend = false;
  _pendingFirmwareUpdate = false;
  _pendingFirmwareVersion = 0;
  _pendingFirmwareDownloadUrl = "";

  _ready = _prefs.begin(kNamespace, false);
  if (!_ready) {
    Serial.println("[Settings] Preferences init failed; using defaults");
    return;
  }

  _brightness =
      constrain(_prefs.getUChar(kBrightnessKey, DEFAULT_BRIGHTNESS), 0, 255);
  _volume = constrain(_prefs.getUChar(kVolumeKey, DEFAULT_VOLUME), 0, 255);
  _chatId = _prefs.getString(kChatIdKey, "");
  _useExternalSpeaker = _prefs.getBool(kExternalSpeakerKey, false);
  _externalSpeakerGain = constrain(
      static_cast<int>(_prefs.getUChar(kExternalGainKey, kDefaultExternalGain)),
      kMinExternalGain, kMaxExternalGain);
  _voice = _prefs.getString(kVoiceKey, kDefaultVoice);
  _serverEndpointIndex =
      max(0, static_cast<int>(_prefs.getInt(kServerEndpointKey, 0)));

  String savedSsid = _prefs.getString(kWifiSsidKey, "");
  savedSsid.trim();
  if (!savedSsid.isEmpty()) {
    _wifiSsid = savedSsid;
    _wifiPassword = _prefs.getString(kWifiPasswordKey, "");
    _hasSavedWifi = true;
  }

  String savedBackendHost = _prefs.getString(kBackendHostKey, "");
  savedBackendHost.trim();
  const int savedBackendPort = _prefs.getInt(kBackendPortKey, 0);
  if (!savedBackendHost.isEmpty() && isValidPort(savedBackendPort)) {
    _backendHost = savedBackendHost;
    _backendPort = savedBackendPort;
    _hasSavedBackend = true;
  }

  const String savedTheme = _prefs.getString(
      kActiveThemeKey, ThemeManager::identifier(ThemeId::Plain));
  _activeTheme = ThemeManager::identifier(
      ThemeManager::resolveAvailableIdentifier(savedTheme));

  _pendingFirmwareUpdate = _prefs.getBool(kFirmwarePendingKey, false);
  _pendingFirmwareVersion =
      max(0, static_cast<int>(_prefs.getInt(kFirmwareVersionKey, 0)));
  _pendingFirmwareDownloadUrl = _prefs.getString(kFirmwareUrlKey, "");
  if (_voice.isEmpty()) {
    _voice = kDefaultVoice;
  }
  if (_pendingFirmwareUpdate &&
      (_pendingFirmwareVersion <= FIRMWARE_VERSION ||
       _pendingFirmwareDownloadUrl.isEmpty())) {
    clearPendingFirmwareUpdate();
  }

  Serial.printf("[Settings] Loaded brightness=%d volume=%d chat=%s "
                "ext_spk=%d gain=%d voice=%s server=%d wifi=%s backend=%s "
                "theme=%s pending_fw=%d:%d\n",
                _brightness, _volume,
                _chatId.isEmpty() ? "(none)" : _chatId.c_str(),
                _useExternalSpeaker ? 1 : 0, _externalSpeakerGain,
                _voice.c_str(), _serverEndpointIndex,
                _hasSavedWifi ? "saved" : "fallback",
                _hasSavedBackend ? "saved" : "fallback",
                _activeTheme.c_str(),
                _pendingFirmwareUpdate ? 1 : 0, _pendingFirmwareVersion);
}

void SettingsStore::setBrightness(int brightness) {
  _brightness = constrain(brightness, 0, 255);
  if (_ready) {
    _prefs.putUChar(kBrightnessKey, static_cast<uint8_t>(_brightness));
  }
}

void SettingsStore::setVolume(int volume) {
  _volume = constrain(volume, 0, 255);
  if (_ready) {
    _prefs.putUChar(kVolumeKey, static_cast<uint8_t>(_volume));
  }
}

void SettingsStore::setChatId(const String &chatId) {
  _chatId = chatId;
  if (_ready) {
    _prefs.putString(kChatIdKey, _chatId);
  }
}

void SettingsStore::clearChatId() {
  _chatId = "";
  if (_ready) {
    _prefs.remove(kChatIdKey);
  }
}

void SettingsStore::setUseExternalSpeaker(bool enabled) {
  _useExternalSpeaker = enabled;
  if (_ready) {
    _prefs.putBool(kExternalSpeakerKey, _useExternalSpeaker);
  }
}

void SettingsStore::setExternalSpeakerGain(int gain) {
  _externalSpeakerGain = constrain(gain, kMinExternalGain, kMaxExternalGain);
  if (_ready) {
    _prefs.putUChar(kExternalGainKey,
                    static_cast<uint8_t>(_externalSpeakerGain));
  }
}

void SettingsStore::setVoice(const String &voice) {
  _voice = voice.isEmpty() ? String(kDefaultVoice) : voice;
  if (_ready) {
    _prefs.putString(kVoiceKey, _voice);
  }
}

void SettingsStore::setServerEndpointIndex(int endpointIndex) {
  _serverEndpointIndex = max(0, endpointIndex);
  if (_ready) {
    _prefs.putInt(kServerEndpointKey, _serverEndpointIndex);
  }
}

bool SettingsStore::saveWifiCredentials(const String &ssid,
                                        const String &password) {
  String validatedSsid = ssid;
  validatedSsid.trim();
  if (!_ready || validatedSsid.isEmpty()) {
    return false;
  }

  const bool previousSsidExisted = _prefs.isKey(kWifiSsidKey);
  const String previousSsid = _prefs.getString(kWifiSsidKey, "");
  const bool previousPasswordExisted = _prefs.isKey(kWifiPasswordKey);
  const String previousPassword = _prefs.getString(kWifiPasswordKey, "");

  if (!writeVerifiedString(_prefs, kWifiSsidKey, validatedSsid) ||
      !writeVerifiedString(_prefs, kWifiPasswordKey, password)) {
    restoreStoredString(_prefs, kWifiSsidKey, previousSsidExisted,
                        previousSsid);
    restoreStoredString(_prefs, kWifiPasswordKey, previousPasswordExisted,
                        previousPassword);
    return false;
  }

  _wifiSsid = validatedSsid;
  _wifiPassword = password;
  _hasSavedWifi = true;
  return true;
}

bool SettingsStore::applySavedWifi(WiFiService &wifi) const {
  if (!_hasSavedWifi) {
    return false;
  }
  wifi.setPrimaryNetwork(_wifiSsid, _wifiPassword);
  return true;
}

bool SettingsStore::saveBackend(const String &host, int port) {
  String validatedHost = host;
  validatedHost.trim();
  if (!_ready || validatedHost.isEmpty() || !isValidPort(port)) {
    return false;
  }

  const size_t hostWritten = _prefs.putString(kBackendHostKey, validatedHost);
  const size_t portWritten = _prefs.putInt(kBackendPortKey, port);
  if (hostWritten == 0 || portWritten != sizeof(int32_t)) {
    return false;
  }

  _backendHost = validatedHost;
  _backendPort = port;
  _hasSavedBackend = true;
  return true;
}

bool SettingsStore::saveActiveTheme(const String &themeId) {
  ThemeId parsed = ThemeId::Plain;
  if (!_ready || !ThemeManager::tryParseIdentifier(themeId, parsed) ||
      !ThemeManager::isAvailable(parsed)) {
    return false;
  }

  const String canonicalId = ThemeManager::identifier(parsed);
  if (_prefs.putString(kActiveThemeKey, canonicalId) == 0) {
    return false;
  }

  _activeTheme = canonicalId;
  return true;
}

void SettingsStore::setPendingFirmwareUpdate(int version,
                                             const String &downloadUrl) {
  if (version <= FIRMWARE_VERSION || downloadUrl.isEmpty()) {
    clearPendingFirmwareUpdate();
    return;
  }

  _pendingFirmwareUpdate = true;
  _pendingFirmwareVersion = version;
  _pendingFirmwareDownloadUrl = downloadUrl;
  if (_ready) {
    _prefs.putBool(kFirmwarePendingKey, true);
    _prefs.putInt(kFirmwareVersionKey, _pendingFirmwareVersion);
    _prefs.putString(kFirmwareUrlKey, _pendingFirmwareDownloadUrl);
  }
}

void SettingsStore::clearPendingFirmwareUpdate() {
  _pendingFirmwareUpdate = false;
  _pendingFirmwareVersion = 0;
  _pendingFirmwareDownloadUrl = "";
  if (_ready) {
    _prefs.remove(kFirmwarePendingKey);
    _prefs.remove(kFirmwareVersionKey);
    _prefs.remove(kFirmwareUrlKey);
  }
}

void SettingsStore::reset() {
  _brightness = DEFAULT_BRIGHTNESS;
  _volume = DEFAULT_VOLUME;
  _chatId = "";
  _useExternalSpeaker = false;
  _externalSpeakerGain = kDefaultExternalGain;
  _voice = kDefaultVoice;
  _serverEndpointIndex = 0;
  _pendingFirmwareUpdate = false;
  _pendingFirmwareVersion = 0;
  _pendingFirmwareDownloadUrl = "";

  if (_ready) {
    _prefs.remove(kBrightnessKey);
    _prefs.remove(kVolumeKey);
    _prefs.remove(kChatIdKey);
    _prefs.remove(kExternalSpeakerKey);
    _prefs.remove(kExternalGainKey);
    _prefs.remove(kVoiceKey);
    _prefs.remove(kServerEndpointKey);
    _prefs.remove(kFirmwarePendingKey);
    _prefs.remove(kFirmwareVersionKey);
    _prefs.remove(kFirmwareUrlKey);
  }
}
