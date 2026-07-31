#pragma once

#include <Arduino.h>
#include <Preferences.h>

class WiFiService;

/**
 * @brief Persistent user and device settings stored in Preferences.
 */
class SettingsStore {
public:
  /// Initialize storage and load settings, falling back to defaults.
  void init();

  /// Current display brightness.
  int brightness() const { return _brightness; }

  /// Current playback volume.
  int volume() const { return _volume; }

  /// Persisted chat id, retained only for legacy cleanup.
  const String &chatId() const { return _chatId; }

  /// Whether external speaker output is enabled.
  bool useExternalSpeaker() const { return _useExternalSpeaker; }

  /// Current external speaker gain.
  int externalSpeakerGain() const { return _externalSpeakerGain; }

  /// Current Gemini voice identifier.
  const String &voice() const { return _voice; }

  /// Preferred server endpoint index.
  int serverEndpointIndex() const { return _serverEndpointIndex; }

  /// Effective WiFi SSID, including the development fallback when needed.
  const String &wifiSsid() const { return _wifiSsid; }

  /// Effective backend host, including the development fallback when needed.
  const String &backendHost() const { return _backendHost; }

  /// Effective backend port, including the development fallback when needed.
  int backendPort() const { return _backendPort; }

  /// Validated active visual theme identifier.
  const String &activeTheme() const { return _activeTheme; }

  /// Whether valid persisted WiFi credentials replaced the fallback.
  bool hasSavedWifi() const { return _hasSavedWifi; }

  /**
   * @brief Transfer saved credentials directly to the WiFi startup service.
   * @param wifi WiFi service that receives the preferred network.
   * @return True when saved credentials were applied.
   */
  bool applySavedWifi(WiFiService &wifi) const;

  /// Whether a valid persisted backend endpoint replaced the fallback.
  bool hasSavedBackend() const { return _hasSavedBackend; }

  /// Whether an OTA update should be installed on next boot.
  bool pendingFirmwareUpdate() const { return _pendingFirmwareUpdate; }

  /// Firmware version associated with the pending OTA update.
  int pendingFirmwareVersion() const { return _pendingFirmwareVersion; }

  /// Download URL associated with the pending OTA update.
  const String &pendingFirmwareDownloadUrl() const {
    return _pendingFirmwareDownloadUrl;
  }

  /**
   * @brief Set and persist display brightness.
   * @param brightness Brightness level, clamped to 0..255.
   */
  void setBrightness(int brightness);

  /**
   * @brief Set and persist playback volume.
   * @param volume Volume level, clamped to 0..255.
   */
  void setVolume(int volume);

  /**
   * @brief Store the current chat id.
   * @param chatId Conversation id to persist.
   */
  void setChatId(const String &chatId);

  /// Clear any persisted chat id.
  void clearChatId();

  /**
   * @brief Set and persist external speaker routing.
   * @param enabled True to route audio to the external speaker.
   */
  void setUseExternalSpeaker(bool enabled);

  /**
   * @brief Set and persist external speaker gain.
   * @param gain Requested gain, clamped to kMinExternalGain..kMaxExternalGain.
   */
  void setExternalSpeakerGain(int gain);

  /**
   * @brief Set and persist the Gemini voice identifier.
   * @param voice Voice id, or empty to restore the default voice.
   */
  void setVoice(const String &voice);

  /**
   * @brief Set and persist the preferred server endpoint index.
   * @param endpointIndex Endpoint index, clamped to zero or greater.
   */
  void setServerEndpointIndex(int endpointIndex);

  /**
   * @brief Validate and persist WiFi credentials.
   * @param ssid Non-empty network SSID.
   * @param password Network password, which may be empty for an open network.
   * @return True when the settings were valid and persisted.
   */
  bool saveWifiCredentials(const String &ssid, const String &password);

  /**
   * @brief Validate and persist the backend endpoint.
   * @param host Non-empty hostname or address.
   * @param port TCP port in the range 1..65535.
   * @return True when the settings were valid and persisted.
   */
  bool saveBackend(const String &host, int port);

  /**
   * @brief Validate and persist the active theme identifier.
   * @param themeId One of the supported theme identifiers.
   * @return True when the identifier was valid and persisted.
   */
  bool saveActiveTheme(const String &themeId);

  /// Whether a theme identifier is supported by this firmware.
  static bool isValidTheme(const String &themeId);

  /**
   * @brief Record a deferred firmware update for installation on next boot.
   * @param version Firmware version to install.
   * @param downloadUrl OTA binary download URL.
   */
  void setPendingFirmwareUpdate(int version, const String &downloadUrl);

  /// Clear any deferred firmware update state.
  void clearPendingFirmwareUpdate();

  /// Reset device state while retaining connectivity and theme settings.
  void reset();

  /// Default external speaker gain.
  static constexpr int kDefaultExternalGain = 24;

  /// Minimum accepted external speaker gain.
  static constexpr int kMinExternalGain = 1;

  /// Maximum accepted external speaker gain.
  static constexpr int kMaxExternalGain = 64;

  /// Default Gemini voice id.
  static constexpr const char *kDefaultVoice = "Aoede";

private:
  /// Preferences namespace handle.
  Preferences _prefs;

  /// Cached display brightness.
  int _brightness = 80;

  /// Cached playback volume.
  int _volume = 255;

  /// Cached legacy chat id.
  String _chatId;

  /// Cached external speaker routing flag.
  bool _useExternalSpeaker = false;

  /// Cached external speaker gain.
  int _externalSpeakerGain = kDefaultExternalGain;

  /// Cached Gemini voice id.
  String _voice = kDefaultVoice;

  /// Cached preferred server endpoint index.
  int _serverEndpointIndex = 0;

  /// Effective WiFi SSID.
  String _wifiSsid;

  /// Effective WiFi password. Never log or display this value.
  String _wifiPassword;

  /// Effective backend hostname or address.
  String _backendHost;

  /// Effective backend TCP port.
  int _backendPort = 0;

  /// Validated active theme identifier.
  String _activeTheme;

  /// Whether valid persisted WiFi credentials are active.
  bool _hasSavedWifi = false;

  /// Whether a valid persisted backend endpoint is active.
  bool _hasSavedBackend = false;

  /// Cached deferred firmware update flag.
  bool _pendingFirmwareUpdate = false;

  /// Cached deferred firmware version.
  int _pendingFirmwareVersion = 0;

  /// Cached deferred firmware download URL.
  String _pendingFirmwareDownloadUrl;

  /// Whether Preferences storage opened successfully.
  bool _ready = false;

  /// Preferences namespace name.
  static constexpr const char *kNamespace = "chat-stick";

  /// Preferences key for brightness.
  static constexpr const char *kBrightnessKey = "brightness";

  /// Preferences key for volume.
  static constexpr const char *kVolumeKey = "volume";

  /// Preferences key for legacy chat id.
  static constexpr const char *kChatIdKey = "chat_id";

  /// Preferences key for external speaker routing.
  static constexpr const char *kExternalSpeakerKey = "ext_spk";

  /// Preferences key for external speaker gain.
  static constexpr const char *kExternalGainKey = "ext_gain";

  /// Preferences key for Gemini voice id.
  static constexpr const char *kVoiceKey = "voice";

  /// Preferences key for preferred server endpoint index.
  static constexpr const char *kServerEndpointKey = "server_idx";

  /// Preferences key for the WiFi SSID.
  static constexpr const char *kWifiSsidKey = "wifi_ssid";

  /// Preferences key for the WiFi password.
  static constexpr const char *kWifiPasswordKey = "wifi_pass";

  /// Preferences key for the backend hostname or address.
  static constexpr const char *kBackendHostKey = "backend_host";

  /// Preferences key for the backend TCP port.
  static constexpr const char *kBackendPortKey = "backend_port";

  /// Preferences key for the active visual theme.
  static constexpr const char *kActiveThemeKey = "theme";

  /// Safe visual theme used when no valid selection exists.
  static constexpr const char *kDefaultTheme = "nerv";

  /// Preferences key for deferred firmware update flag.
  static constexpr const char *kFirmwarePendingKey = "fw_pending";

  /// Preferences key for deferred firmware version.
  static constexpr const char *kFirmwareVersionKey = "fw_ver";

  /// Preferences key for deferred firmware download URL.
  static constexpr const char *kFirmwareUrlKey = "fw_url";
};
