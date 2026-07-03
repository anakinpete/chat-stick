package com.tldraw.chatstick;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

import javax.net.SocketFactory;
import javax.net.ssl.SSLSocketFactory;

public class MainActivity extends Activity {
	private static final String TAG = "ChatStick";
	private static final String PREFS = "chat-stick";
	private static final String KEY_SERVER_URL = "serverUrl";
	private static final String KEY_DEVICE_ID = "deviceId";
	private static final String KEY_DEVICE_TOKEN = "deviceToken";
	private static final String DEFAULT_SERVER_URL = "http://127.0.0.1:8788";
	private static final String PRODUCTION_SERVER_URL = "https://m5-live.tldraw.workers.dev";
	private static final String DEFAULT_VOICE = "Aoede";
	private static final int IMAGE_REQUEST_W = 360;
	private static final int IMAGE_REQUEST_H = 360;
	private static final String IDLE_TEXT =
		"Hi, how can I help?";

	private final Handler main = new Handler(Looper.getMainLooper());
	private final ExecutorService io = Executors.newCachedThreadPool();

	private SharedPreferences prefs;
	private ChatStickDisplayView displayView;

	private ChatWebSocket socket;
	private AudioCapture capture;
	private AudioOutput audioOutput;
	private TimerStore timerStore;
	private String chatId = "";
	private String lastTranscriptSource = "";
	private String assistantTranscriptText = "";
	private boolean aButtonHeld = false;
	private boolean connecting = false;
	private boolean recording = false;
	private boolean assistantSpeaking = false;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		super.onCreate(savedInstanceState);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
		configureAudioForChatStick();
		audioOutput = new AudioOutput();
		timerStore = new TimerStore(this, main, this::onTimerExpired);
		buildUi();
		requestAudioPermission();
		connect();
	}

	@Override
	protected void onResume() {
		super.onResume();
		configureAudioForChatStick();
		enterImmersiveMode();
		if (displayView != null) {
			focusDisplayView();
		}
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus) {
			enterImmersiveMode();
			focusDisplayView();
		}
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		stopRecording();
		disconnect();
		timerStore.close();
		audioOutput.close();
		io.shutdownNow();
	}

	private void buildUi() {
		displayView = new ChatStickDisplayView(this, event -> handleButtonKeyEvent(event.getKeyCode(), event));
		displayView.setFocusable(true);
		displayView.setFocusableInTouchMode(true);
		displayView.setOnKeyListener((view, keyCode, event) -> handleButtonKeyEvent(keyCode, event));
		displayView.showIdle(IDLE_TEXT);
		setContentView(displayView);
		enterImmersiveMode();
		focusDisplayView();
	}

	private void focusDisplayView() {
		if (displayView == null) return;
		displayView.requestFocus();
		main.postDelayed(() -> {
			if (displayView == null) return;
			displayView.requestFocus();
		}, 100);
	}

	private void enterImmersiveMode() {
		getWindow().getDecorView().setSystemUiVisibility(
			View.SYSTEM_UI_FLAG_FULLSCREEN
				| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
				| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
				| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
				| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
				| View.SYSTEM_UI_FLAG_LAYOUT_STABLE
		);
	}

	private String defaultDeviceId() {
		String model = Build.MODEL == null ? "sidephone" : Build.MODEL;
		return ("sidephone-" + model).toLowerCase(Locale.US).replaceAll("[^a-z0-9._-]+", "-");
	}

	private void requestAudioPermission() {
		if (Build.VERSION.SDK_INT >= 23 && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
			requestPermissions(new String[] { Manifest.permission.RECORD_AUDIO }, 7);
		}
	}

	private void configureAudioForChatStick() {
		AudioManager audio = (AudioManager) getSystemService(AUDIO_SERVICE);
		if (audio == null) return;
		try {
			audio.setMode(AudioManager.MODE_NORMAL);
			audio.setSpeakerphoneOn(true);
		} catch (Exception ignored) {
		}
	}

	private void connect() {
		if (connecting) {
			setStatus("Connecting...");
			return;
		}
		String serverUrl = prefs.getString(KEY_SERVER_URL, DEFAULT_SERVER_URL).trim();
		String deviceId = prefs.getString(KEY_DEVICE_ID, defaultDeviceId()).trim();
		String token = prefs.getString(KEY_DEVICE_TOKEN, "").trim();
		if (serverUrl.isEmpty()) serverUrl = DEFAULT_SERVER_URL;
		if (deviceId.isEmpty()) deviceId = defaultDeviceId();
		List<String> serverUrls = serverUrlCandidates(serverUrl);

		prefs.edit()
			.putString(KEY_SERVER_URL, serverUrls.get(0))
			.putString(KEY_DEVICE_ID, deviceId)
			.putString(KEY_DEVICE_TOKEN, token)
			.apply();

		connectToServer(serverUrls, 0, deviceId, token);
	}

	private List<String> serverUrlCandidates(String preferredUrl) {
		List<String> urls = new ArrayList<>();
		addServerUrl(urls, preferredUrl);
		addServerUrl(urls, DEFAULT_SERVER_URL);
		addServerUrl(urls, PRODUCTION_SERVER_URL);
		return urls;
	}

	private void addServerUrl(List<String> urls, String url) {
		if (url == null) return;
		String value = url.trim();
		if (value.isEmpty()) return;
		for (String existing : urls) {
			if (existing.equals(value)) return;
		}
		urls.add(value);
	}

	private void connectToServer(List<String> serverUrls, int endpointIndex, String deviceId, String token) {
		if (endpointIndex >= serverUrls.size()) {
			connecting = false;
			setStatus("Connection error");
			return;
		}
		String serverUrl = serverUrls.get(endpointIndex);
		URI uri;
		try {
			uri = websocketUri(serverUrl, deviceId);
		} catch (Exception e) {
			if (endpointIndex + 1 < serverUrls.size()) {
				connectToServer(serverUrls, endpointIndex + 1, deviceId, token);
			} else {
				connecting = false;
				setStatus("Bad server URL: " + e.getMessage());
			}
			return;
		}

		disconnect();
		connecting = true;
		setStatus("Connecting...");
		final ChatWebSocket[] socketRef = new ChatWebSocket[1];
		ChatWebSocket currentSocket = new ChatWebSocket(uri, token, new ChatWebSocket.Listener() {
			@Override
			public void onOpen() {
				main.post(() -> {
					if (socket != thisSocket()) return;
					connecting = false;
					setStatus("Connected");
					if (aButtonHeld) {
						startRecording();
					}
				});
			}

			@Override
			public void onText(String text) {
				main.post(() -> {
					if (socket == thisSocket()) {
						handleServerText(text);
					}
				});
			}

			@Override
			public void onBinary(byte[] data) {
				if (socket != thisSocket()) return;
				assistantSpeaking = true;
				configureAudioForChatStick();
				audioOutput.play(data, () -> assistantSpeaking = false);
			}

			@Override
			public void onClosed(String reason) {
				main.post(() -> {
					if (socket != thisSocket()) return;
					connecting = false;
					setStatus("Disconnected" + (reason.isEmpty() ? "" : ": " + reason));
				});
			}

			@Override
			public void onError(Exception error) {
				main.post(() -> {
					if (socket != thisSocket()) return;
					connecting = false;
					socket = null;
					if (endpointIndex + 1 < serverUrls.size()) {
						connectToServer(serverUrls, endpointIndex + 1, deviceId, token);
					} else {
						setStatus("Connection error: " + error.getMessage());
					}
				});
			}

			private ChatWebSocket thisSocket() {
				return socketRef[0];
			}
		});
		socketRef[0] = currentSocket;
		socket = currentSocket;
		io.execute(() -> currentSocket.connect());
	}

	private URI websocketUri(String serverUrl, String deviceId) throws Exception {
		URI base = new URI(serverUrl);
		String scheme = base.getScheme();
		if (scheme == null) scheme = "http";
		if ("http".equals(scheme)) scheme = "ws";
		else if ("https".equals(scheme)) scheme = "wss";
		else if (!"ws".equals(scheme) && !"wss".equals(scheme)) {
			throw new IllegalArgumentException("Use http, https, ws, or wss");
		}

		String path = base.getPath();
		if (path == null || path.isEmpty() || "/".equals(path)) {
			path = "/ws";
		} else if (!path.endsWith("/ws")) {
			path = path.replaceAll("/+$", "") + "/ws";
		}

		String query = "device_id=" + urlEncode(deviceId) +
			"&voice=" + urlEncode(DEFAULT_VOICE) +
			"&image_w=" + IMAGE_REQUEST_W +
			"&image_h=" + IMAGE_REQUEST_H;
		if (base.getQuery() != null && !base.getQuery().isEmpty()) {
			query = base.getQuery() + "&" + query;
		}
		return new URI(scheme, base.getUserInfo(), base.getHost(), base.getPort(), path, query, null);
	}

	private String urlEncode(String value) {
		try {
			return java.net.URLEncoder.encode(value, "UTF-8");
		} catch (Exception e) {
			return value;
		}
	}

	@Override
	public boolean dispatchKeyEvent(KeyEvent event) {
		Log.d(TAG, "dispatch keyCode=" + event.getKeyCode() + " action=" + event.getAction() + " repeat=" + event.getRepeatCount());
		if (handleButtonKeyEvent(event.getKeyCode(), event)) return true;
		return super.dispatchKeyEvent(event);
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (handleButtonKeyEvent(keyCode, event)) return true;
		return super.onKeyDown(keyCode, event);
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event) {
		if (handleButtonKeyEvent(keyCode, event)) return true;
		return super.onKeyUp(keyCode, event);
	}

	private boolean handleButtonKeyEvent(int keyCode, KeyEvent event) {
		if (handleTalkKeyEvent(keyCode, event)) return true;
		if (!isPageKey(keyCode)) return false;
		if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0 && displayView != null) {
			displayView.pressB();
		}
		return true;
	}

	private boolean handleTalkKeyEvent(int keyCode, KeyEvent event) {
		if (!isTalkKey(keyCode)) return false;
		Log.d(TAG, "talk key action=" + event.getAction() + " repeat=" + event.getRepeatCount());
		if (event.getAction() == KeyEvent.ACTION_DOWN) {
			aButtonHeld = true;
			if (event.getRepeatCount() == 0) startRecording();
			return true;
		}
		if (event.getAction() == KeyEvent.ACTION_UP) {
			aButtonHeld = false;
			stopRecording();
			return true;
		}
		return true;
	}

	private boolean isTalkKey(int keyCode) {
		return keyCode == KeyEvent.KEYCODE_Q
			|| keyCode == KeyEvent.KEYCODE_DPAD_UP
			|| keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
			|| keyCode == KeyEvent.KEYCODE_HEADSETHOOK;
	}

	private boolean isPageKey(int keyCode) {
		return keyCode == KeyEvent.KEYCODE_E
			|| keyCode == KeyEvent.KEYCODE_DPAD_DOWN
			|| keyCode == KeyEvent.KEYCODE_DPAD_RIGHT;
	}

	private interface DisplayInputHandler {
		boolean handleKeyEvent(KeyEvent event);
	}

	private void disconnect() {
		if (socket != null) {
			socket.close("client disconnect");
			socket = null;
		}
	}

	private void startRecording() {
		if (recording) return;
		interruptAssistantPlayback();
		if (Build.VERSION.SDK_INT >= 23 && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
			requestAudioPermission();
			setStatus("Microphone permission needed");
			return;
		}
		ChatWebSocket ws = socket;
		if (ws == null || !ws.isOpen()) {
			setStatus("Connecting...");
			if (!connecting) {
				connect();
			}
			return;
		}
		recording = true;
		if (displayView != null) {
			displayView.setRecordingMode(true);
		}
		assistantTranscriptText = "";
		lastTranscriptSource = "";
		capture = new AudioCapture(ws, () -> main.post(() -> setStatus("Recording failed")));
		capture.start();
	}

	private void stopRecording() {
		if (!recording) return;
		recording = false;
		if (displayView != null) {
			displayView.setRecordingMode(false);
		}
		setStatus("Thinking");
		if (capture != null) {
			capture.stop();
			capture = null;
		}
	}

	private void handleServerText(String text) {
		try {
			JSONObject msg = new JSONObject(text);
			String type = msg.optString("type", "");
			if ("session".equals(type)) {
				chatId = msg.optString("chatId", chatId);
				setStatus("Session " + shortId(chatId));
				return;
			}
			if ("server_ready".equals(type)) {
				setStatus("Device channel ready");
				return;
			}
			if ("ready".equals(type)) {
				setStatus("Agent ready");
				return;
			}
			if ("turn_complete".equals(type)) {
				lastTranscriptSource = "";
				setStatus("Ready");
				return;
			}
			if ("drop_audio".equals(type)) {
				interruptAssistantPlayback();
				setStatus("Interrupted");
				return;
			}
			if ("ignore_audio".equals(type)) {
				setStatus("Ignored audio: " + msg.optString("reason", "too short"));
				return;
			}
			if ("transcript".equals(type)) {
				appendTranscript(msg.optString("source", ""), msg.optString("text", ""));
				return;
			}
			if ("tool_call".equals(type)) {
				handleToolCall(msg);
				return;
			}
			if ("voice_changed".equals(type)) {
				setStatus("Voice: " + msg.optString("voice", ""));
				return;
			}
			if ("show_image_pending".equals(type)) {
				setStatus("Generating image...");
				return;
			}
			if ("show_image".equals(type)) {
				showPackedImage(msg);
				return;
			}
			if ("show_image_failed".equals(type)) {
				setStatus("Image generation failed");
				return;
			}
			if ("error".equals(type)) {
				String message = msg.optString("message", "Server error");
				setStatus(message);
			}
		} catch (JSONException e) {
			appendLine("Server: " + text);
		}
	}

	private String shortId(String value) {
		if (value == null || value.length() <= 8) return value == null ? "" : value;
		return value.substring(0, 8);
	}

	private void appendTranscript(String source, String text) {
		if (text.isEmpty()) return;
		if (!source.equals(lastTranscriptSource)) {
			lastTranscriptSource = source;
		}
		if ("model".equals(source)) {
			assistantTranscriptText += text;
			if (displayView != null) {
				displayView.setToolText(assistantTranscriptText);
			}
		}
	}

	private void interruptAssistantPlayback() {
		assistantSpeaking = false;
		if (audioOutput != null) {
			audioOutput.interrupt();
		}
	}

	private void appendLine(String line) {
		if (displayView != null) {
			displayView.showTransientText(line);
		}
	}

	private void setStatus(String status) {
		if (displayView == null) return;
		if ("Recording".equals(status)) {
			displayView.setRecordingMode(true);
			return;
		}
		if ("Thinking".equals(status) || "Answering".equals(status)) {
			displayView.showTransientCentered("Thinking...");
			return;
		}
		if ("Connecting...".equals(status)) {
			displayView.showTransientCentered("Connecting...");
			return;
		}
		if ("Generating image...".equals(status)) {
			displayView.showTransientCentered("Thinking...");
			return;
		}
		if ("Connected".equals(status)
			|| "Ready".equals(status)
			|| "Agent ready".equals(status)
			|| "Device channel ready".equals(status)
			|| status.startsWith("Session ")
			|| status.startsWith("Voice:")) {
			displayView.showIdleIfEmpty();
			return;
		}
		displayView.showTransientText(status);
	}

	private void showPackedImage(JSONObject msg) {
		int width = msg.optInt("width", 0);
		int height = msg.optInt("height", 0);
		String data = msg.optString("data", "");
		if (width <= 0 || height <= 0 || data.isEmpty()) {
			setStatus("Image payload missing");
			return;
		}
		try {
			byte[] packed = Base64.decode(data, Base64.DEFAULT);
			Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
			int[] pixels = new int[width * height];
			for (int i = 0; i < pixels.length; i++) {
				int b = packed[i / 8] & 0xff;
				boolean on = (b & (1 << (7 - (i % 8)))) != 0;
				pixels[i] = on ? Color.WHITE : Color.BLACK;
			}
			bitmap.setPixels(pixels, 0, width, 0, 0, width, height);
			if (displayView != null) {
				displayView.setToolImage(bitmap);
			}
			setStatus("Image shown");
		} catch (Exception e) {
			setStatus("Image decode failed");
		}
	}

	private void handleToolCall(JSONObject msg) {
		String name = msg.optString("name", "");
		String id = msg.optString("id", "");
		JSONObject args = msg.optJSONObject("args");
		if (args == null) args = new JSONObject();
		String result;
		try {
			switch (name) {
				case "get_device_status":
					result = getDeviceStatus().toString();
					break;
				case "set_volume":
					result = setVolume(args.optInt("level", 128));
					break;
				case "set_brightness":
					result = setBrightness(args.optInt("level", 180));
					break;
				case "set_speaker":
					result = "Speaker routing is fixed to the Sidephone speaker";
					break;
				case "set_external_speaker_gain":
					result = "External speaker gain is unavailable on Sidephone";
					break;
				case "show_text":
					String displayText = args.optString("text", "");
					if (displayView != null) {
						displayView.setToolText(displayText);
					}
					result = "Displayed text";
					break;
				case "play_sound":
					String sound = args.optString("sound", "beep");
					result = playSound(sound) ? "Played sound: " + sound : "Unknown sound";
					break;
				case "play_melody":
					result = playMelody(args.optString("notes", "")) ? "Melody played" : "Invalid melody";
					break;
				case "power_off":
					result = "Power off is unavailable from the Android app";
					break;
				case "set_timer":
					result = timerStore.add(args.optInt("duration_seconds", 0), args.optString("name", ""));
					break;
				case "list_timers":
					result = timerStore.list();
					break;
				case "cancel_timer":
					result = timerStore.cancel(args);
					break;
				case "extend_timer":
					result = timerStore.extend(args);
					break;
				default:
					result = name + " is not implemented on Sidephone";
			}
		} catch (Exception e) {
			result = "Tool failed: " + e.getMessage();
		}
		sendToolResponse(name, id, result);
	}

	private JSONObject getDeviceStatus() throws JSONException {
		AudioManager audio = (AudioManager) getSystemService(AUDIO_SERVICE);
		JSONObject status = new JSONObject();
		status.put("device", "sidephone");
		status.put("app", "sidephone-chat-stick");
		status.put("app_version", "0.1.0");
		status.put("chat_id", chatId);
		status.put("android_model", Build.MODEL);
		status.put("android_version", Build.VERSION.RELEASE);
		status.put("sdk", Build.VERSION.SDK_INT);
		status.put("recording", recording);
		status.put("assistant_speaking", assistantSpeaking);
		status.put("battery_percent", batteryPercent());
		status.put("media_volume", audio.getStreamVolume(AudioManager.STREAM_MUSIC));
		status.put("media_volume_max", audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC));
		status.put("timers", new JSONObject(timerStore.list()).optJSONArray("timers"));
		return status;
	}

	private int batteryPercent() {
		BatteryManager battery = (BatteryManager) getSystemService(BATTERY_SERVICE);
		if (battery == null || Build.VERSION.SDK_INT < 21) return -1;
		return battery.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY);
	}

	private String setVolume(int level) {
		int bounded = Math.max(0, Math.min(255, level));
		AudioManager audio = (AudioManager) getSystemService(AUDIO_SERVICE);
		int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
		int target = Math.round(bounded * max / 255f);
		audio.setStreamVolume(AudioManager.STREAM_MUSIC, target, 0);
		return "Volume set to " + bounded;
	}

	private String setBrightness(int level) {
		int bounded = Math.max(0, Math.min(255, level));
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = bounded / 255f;
		getWindow().setAttributes(params);
		return "Brightness set to " + bounded;
	}

	private void playTone() {
		playSound("beep");
	}

	private boolean playSound(String sound) {
		if (audioOutput == null) return false;
		String normalized = sound == null ? "" : sound.trim().toLowerCase(Locale.US);
		if (normalized.isEmpty() || "beep".equals(normalized)) {
			return audioOutput.playToneSequence("C6:180");
		}
		if ("success".equals(normalized)) {
			return audioOutput.playToneSequence("C6:90 E6:90 G6:140");
		}
		if ("error".equals(normalized)) {
			return audioOutput.playToneSequence("G4:120 R:40 C4:240");
		}
		if ("alert".equals(normalized)) {
			return audioOutput.playToneSequence("A5:120 R:40 A5:120 R:40 A5:200");
		}
		if ("fanfare".equals(normalized)) {
			return audioOutput.playToneSequence("C5:100 E5:100 G5:100 C6:260");
		}
		return false;
	}

	private boolean playMelody(String notes) {
		return audioOutput != null && audioOutput.playToneSequence(notes);
	}

	private void onTimerExpired(TimerStore.Timer timer) {
		playTone();
		if (displayView != null) {
			displayView.setToolText("Timer expired: " + (timer.name.isEmpty() ? "#" + timer.id : timer.name));
		}
	}

	private void sendToolResponse(String name, String id, String result) {
		ChatWebSocket ws = socket;
		if (ws == null || !ws.isOpen()) return;
		try {
			JSONObject response = new JSONObject();
			response.put("type", "tool_response");
			response.put("name", name);
			response.put("id", id);
			response.put("result", result);
			ws.sendText(response.toString());
		} catch (JSONException e) {
			setStatus("Tool response failed");
		}
	}

	private static final class ChatStickDisplayView extends View {
		private static final int SCREEN_W = 480;
		private static final int SCREEN_H = 640;
		private static final int TEXT_X = 12;
		private static final int TEXT_Y = 28;
		private static final int CHAR_W = 10;
		private static final int LINE_H = 19;
		private static final int BODY_COLS = 45;
		private static final int BODY_LINES = 30;
		private static final int TOTAL_LINES = 31;
		private static final float BASELINE_OFFSET = 15f;
		private static final int IMAGE_MAX_W = 360;
		private static final int IMAGE_MAX_H = 360;
		private static final int COLOR_BG = Color.BLACK;
		private static final int COLOR_TEXT = Color.WHITE;
		private static final int COLOR_MUTED = Color.rgb(160, 160, 160);

		private final Paint bgPaint = new Paint();
		private final Paint textPaint = new Paint();
		private final Paint imagePaint = new Paint();
		private final Rect srcRect = new Rect();
		private final RectF dstRect = new RectF();
		private final List<DisplayPage> pages = new ArrayList<>();
		private final DisplayInputHandler inputHandler;

		private String idleText = "";
		private String toolText = "";
		private Bitmap toolImage;
		private int pageIndex = 0;
		private boolean persistentContent = false;
		private boolean recordingMode = false;

		ChatStickDisplayView(Context context, DisplayInputHandler inputHandler) {
			super(context);
			this.inputHandler = inputHandler;
			bgPaint.setColor(COLOR_BG);
			textPaint.setColor(COLOR_TEXT);
			textPaint.setTypeface(Typeface.MONOSPACE);
			textPaint.setTextSize(16f);
			textPaint.setAntiAlias(false);
			float measured = textPaint.measureText("M");
			if (measured > 0) {
				textPaint.setTextScaleX(CHAR_W / measured);
			}
			imagePaint.setFilterBitmap(false);
			imagePaint.setDither(false);
			setBackgroundColor(COLOR_BG);
		}

		@Override
		public boolean dispatchKeyEvent(KeyEvent event) {
			if (inputHandler != null && inputHandler.handleKeyEvent(event)) return true;
			return super.dispatchKeyEvent(event);
		}

		@Override
		public boolean onKeyPreIme(int keyCode, KeyEvent event) {
			if (inputHandler != null && inputHandler.handleKeyEvent(event)) return true;
			return super.onKeyPreIme(keyCode, event);
		}

		void showIdle(String text) {
			idleText = sanitizeText(text);
			if (!hasPersistentContent()) {
				setTransientText(idleText, false, COLOR_TEXT);
			}
		}

		void showIdleIfEmpty() {
			if (!hasPersistentContent()) {
				setTransientText(idleText, false, COLOR_TEXT);
			}
		}

		void setRecordingMode(boolean recording) {
			recordingMode = recording;
			invalidate();
		}

		void showTransientCentered(String text) {
			if (hasPersistentContent()) return;
			setTransientText(text, true, COLOR_MUTED);
		}

		void showTransientText(String text) {
			if (hasPersistentContent()) return;
			setTransientText(text, false, COLOR_TEXT);
		}

		void clearPersistentContent() {
			toolText = "";
			toolImage = null;
			persistentContent = false;
			showIdleIfEmpty();
		}

		void setToolText(String text) {
			toolText = sanitizeText(text).trim();
			if (toolImage == null && toolText.isEmpty()) {
				clearPersistentContent();
				return;
			}
			persistentContent = true;
			rebuildToolPages();
		}

		void setToolImage(Bitmap bitmap) {
			toolImage = bitmap;
			persistentContent = true;
			rebuildToolPages();
		}

		boolean pressB() {
			if (pages.size() > 1 && pageIndex < pages.size() - 1) {
				pageIndex++;
				invalidate();
				return true;
			}
			if (hasPersistentContent()) {
				clearPersistentContent();
				return true;
			}
			return false;
		}

		@Override
		protected void onDraw(Canvas canvas) {
			canvas.drawColor(COLOR_BG);
			if (getWidth() <= 0 || getHeight() <= 0) return;
			float scale = Math.min(getWidth() / (float) SCREEN_W, getHeight() / (float) SCREEN_H);
			float left = (getWidth() - SCREEN_W * scale) / 2f;
			float top = (getHeight() - SCREEN_H * scale) / 2f;

			canvas.save();
			canvas.translate(left, top);
			canvas.scale(scale, scale);
			canvas.drawRect(0, 0, SCREEN_W, SCREEN_H, bgPaint);

			DisplayPage page = pages.isEmpty() ? DisplayPage.blank(COLOR_TEXT) : pages.get(Math.max(0, Math.min(pageIndex, pages.size() - 1)));
			if (page.image != null) {
				srcRect.set(0, 0, page.image.getWidth(), page.image.getHeight());
				setCenteredImageRect(page.image);
				imagePaint.setAlpha(recordingMode ? 120 : 255);
				canvas.drawBitmap(page.image, srcRect, dstRect, imagePaint);
				imagePaint.setAlpha(255);
			} else {
				textPaint.setColor(recordingMode ? COLOR_MUTED : page.textColor);
				for (int i = 0; i < TOTAL_LINES; i++) {
					drawLine(canvas, page.lines[i], i);
				}
			}
			if (page.hasFooter) {
				textPaint.setColor(COLOR_MUTED);
				drawLine(canvas, pageIndex < pages.size() - 1 ? "v" : "o", TOTAL_LINES - 1, BODY_COLS - 1);
			}
			canvas.restore();
		}

		private void setCenteredImageRect(Bitmap image) {
			float availableW = Math.min(IMAGE_MAX_W, SCREEN_W - TEXT_X * 2f);
			float availableH = Math.min(IMAGE_MAX_H, BODY_LINES * LINE_H);
			float imageW = Math.max(1, image.getWidth());
			float imageH = Math.max(1, image.getHeight());
			float imageScale = Math.min(availableW / imageW, availableH / imageH);
			float drawW = imageW * imageScale;
			float drawH = imageH * imageScale;
			float left = (SCREEN_W - drawW) / 2f;
			float top = TEXT_Y + (BODY_LINES * LINE_H - drawH) / 2f;
			dstRect.set(left, top, left + drawW, top + drawH);
		}

		private void drawLine(Canvas canvas, String line, int row) {
			drawLine(canvas, line, row, 0);
		}

		private void drawLine(Canvas canvas, String line, int row, int col) {
			if (line == null || line.isEmpty()) return;
			canvas.drawText(fitLine(line), TEXT_X + col * CHAR_W, TEXT_Y + BASELINE_OFFSET + row * LINE_H, textPaint);
		}

		private void setTransientText(String text, boolean centered, int color) {
			List<String> lines = wrapText(text);
			DisplayPage page = DisplayPage.blank(color);
			if (centered) {
				int count = Math.min(lines.size(), TOTAL_LINES);
				int first = Math.max(0, (TOTAL_LINES - count) / 2);
				for (int i = 0; i < count; i++) {
					page.lines[first + i] = lines.get(i);
				}
			} else {
				for (int i = 0; i < Math.min(lines.size(), TOTAL_LINES); i++) {
					page.lines[i] = lines.get(i);
				}
			}
			pages.clear();
			pages.add(page);
			pageIndex = 0;
			invalidate();
		}

		private void rebuildToolPages() {
			pages.clear();
			if (toolImage != null) {
				DisplayPage imagePage = DisplayPage.blank(COLOR_TEXT);
				imagePage.image = toolImage;
				imagePage.hasFooter = true;
				pages.add(imagePage);
			}
			if (!toolText.isEmpty()) {
				List<String> lines = wrapText(toolText);
				for (int start = 0; start < lines.size(); start += BODY_LINES) {
					DisplayPage page = DisplayPage.blank(COLOR_TEXT);
					page.hasFooter = true;
					for (int row = 0; row < BODY_LINES && start + row < lines.size(); row++) {
						page.lines[row] = lines.get(start + row);
					}
					pages.add(page);
				}
			}
			if (pages.isEmpty()) {
				persistentContent = false;
				setTransientText(idleText, false, COLOR_TEXT);
				return;
			}
			pageIndex = 0;
			invalidate();
		}

		private boolean hasPersistentContent() {
			return persistentContent && (toolImage != null || !toolText.isEmpty());
		}

		private static List<String> wrapText(String value) {
			List<String> lines = new ArrayList<>();
			String clean = sanitizeText(value);
			String[] paragraphs = clean.split("\\n", -1);
			for (String paragraph : paragraphs) {
				String remaining = paragraph.trim().replaceAll(" +", " ");
				if (remaining.isEmpty()) {
					lines.add("");
					continue;
				}
				while (remaining.length() > BODY_COLS) {
					int breakAt = -1;
					for (int i = BODY_COLS; i > 0; i--) {
						if (remaining.charAt(i - 1) == ' ') {
							breakAt = i - 1;
							break;
						}
					}
					if (breakAt <= 0) {
						lines.add(remaining.substring(0, BODY_COLS));
						remaining = remaining.substring(BODY_COLS).trim();
					} else {
						lines.add(remaining.substring(0, breakAt).trim());
						remaining = remaining.substring(breakAt + 1).trim();
					}
				}
				lines.add(remaining);
			}
			if (lines.isEmpty()) lines.add("");
			return lines;
		}

		private static String sanitizeText(String value) {
			if (value == null) return "";
			String normalized = value
				.replace('\u2018', '\'')
				.replace('\u2019', '\'')
				.replace('\u201c', '"')
				.replace('\u201d', '"')
				.replace("\u2013", "-")
				.replace("\u2014", "-")
				.replace("\u2026", "...")
				.replace("\u25be", "v")
				.replace("\u25cf", "o")
				.replace("\u25b8", ">");
			StringBuilder out = new StringBuilder(normalized.length());
			for (int i = 0; i < normalized.length(); i++) {
				char c = normalized.charAt(i);
				if (c == '\n' || (c >= 32 && c <= 126)) {
					out.append(c);
				} else if (Character.isWhitespace(c)) {
					out.append(' ');
				} else {
					out.append('?');
				}
			}
			return out.toString();
		}

		private static String fitLine(String line) {
			if (line.length() <= BODY_COLS) return line;
			return line.substring(0, BODY_COLS);
		}

		private static final class DisplayPage {
			final String[] lines = new String[TOTAL_LINES];
			final int textColor;
			Bitmap image;
			boolean hasFooter = false;

			private DisplayPage(int textColor) {
				this.textColor = textColor;
				for (int i = 0; i < lines.length; i++) {
					lines[i] = "";
				}
			}

			static DisplayPage blank(int textColor) {
				return new DisplayPage(textColor);
			}
		}
	}

	private static final class AudioCapture {
		private static final int MIC_GAIN = 12;
		private final ChatWebSocket socket;
		private final Runnable onError;
		private volatile boolean running = false;
		private AudioRecord recorder;
		private Thread thread;

		AudioCapture(ChatWebSocket socket, Runnable onError) {
			this.socket = socket;
			this.onError = onError;
		}

		void start() {
			int min = AudioRecord.getMinBufferSize(16000, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
			int bufferSize = Math.max(min, 4096);
			recorder = new AudioRecord(
				MediaRecorder.AudioSource.VOICE_RECOGNITION,
				16000,
				AudioFormat.CHANNEL_IN_MONO,
				AudioFormat.ENCODING_PCM_16BIT,
				bufferSize
			);
			running = true;
			socket.sendText("{\"type\":\"start\"}");
			thread = new Thread(() -> {
				byte[] buffer = new byte[bufferSize];
				try {
					recorder.startRecording();
					while (running && socket.isOpen()) {
						int read = recorder.read(buffer, 0, buffer.length);
						if (running && read > 0) {
							byte[] copy = new byte[read];
							System.arraycopy(buffer, 0, copy, 0, read);
							applyMicGain(copy, read);
							socket.sendBinary(copy);
						}
					}
				} catch (Exception e) {
					onError.run();
				} finally {
					try {
						recorder.stop();
					} catch (Exception ignored) {
					}
					recorder.release();
				}
			}, "chat-stick-audio-capture");
			thread.start();
		}

		void stop() {
			running = false;
			if (recorder != null) {
				try {
					recorder.stop();
				} catch (Exception ignored) {
				}
			}
			socket.sendText("{\"type\":\"stop\"}");
		}

		private static void applyMicGain(byte[] data, int length) {
			for (int i = 0; i + 1 < length; i += 2) {
				int sample = (short) (((data[i + 1] & 0xff) << 8) | (data[i] & 0xff));
				int boosted = sample * MIC_GAIN;
				if (boosted > 32767) boosted = 32767;
				if (boosted < -32768) boosted = -32768;
				data[i] = (byte) (boosted & 0xff);
				data[i + 1] = (byte) ((boosted >> 8) & 0xff);
			}
		}
	}

	private static final class AudioOutput implements Closeable {
		private static final int SAMPLE_RATE = 24000;
		private static final int MAX_SEQUENCE_MS = 10000;
		private static final int DEFAULT_NOTE_MS = 180;
		private static final int MIN_NOTE_MS = 20;
		private static final int MAX_NOTE_MS = 3000;
		private static final double TONE_AMPLITUDE = 0.32;

		private final ExecutorService executor = Executors.newSingleThreadExecutor();
		private final Object trackLock = new Object();
		private final AudioTrack track;
		private int generation = 0;

		AudioOutput() {
			int min = AudioTrack.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);
			track = new AudioTrack(
				AudioManager.STREAM_MUSIC,
				SAMPLE_RATE,
				AudioFormat.CHANNEL_OUT_MONO,
				AudioFormat.ENCODING_PCM_16BIT,
				Math.max(min * 4, 8192),
				AudioTrack.MODE_STREAM
			);
			track.play();
		}

		void play(byte[] data, Runnable onDone) {
			final int playGeneration;
			synchronized (trackLock) {
				playGeneration = generation;
			}
			executor.execute(() -> {
				try {
					synchronized (trackLock) {
						if (playGeneration != generation) return;
						track.write(data, 0, data.length);
					}
				} finally {
					onDone.run();
				}
			});
		}

		boolean playToneSequence(String sequence) {
			byte[] pcm = renderToneSequence(sequence);
			if (pcm == null || pcm.length == 0) return false;
			play(pcm, () -> {});
			return true;
		}

		void flush() {
			interrupt();
		}

		void interrupt() {
			synchronized (trackLock) {
				generation++;
				try {
					track.pause();
					track.flush();
					track.play();
				} catch (Exception ignored) {
				}
			}
		}

		@Override
		public void close() {
			executor.shutdownNow();
			try {
				track.stop();
			} catch (Exception ignored) {
			}
			track.release();
		}

		private static byte[] renderToneSequence(String sequence) {
			if (sequence == null) return null;
			String trimmed = sequence.trim();
			if (trimmed.isEmpty()) return null;
			String[] tokens = trimmed.split("[\\s,]+");
			ByteArrayOutputStream out = new ByteArrayOutputStream();
			int totalMs = 0;
			boolean renderedAny = false;
			for (String rawToken : tokens) {
				if (rawToken.isEmpty()) continue;
				Note note = parseNoteToken(rawToken);
				if (note == null) return null;
				totalMs += note.durationMs;
				if (totalMs > MAX_SEQUENCE_MS) return null;
				appendTone(out, note.frequencyHz, note.durationMs);
				renderedAny = true;
			}
			return renderedAny ? out.toByteArray() : null;
		}

		private static Note parseNoteToken(String token) {
			String notePart = token;
			int durationMs = DEFAULT_NOTE_MS;
			int colon = token.indexOf(':');
			if (colon >= 0) {
				notePart = token.substring(0, colon);
				try {
					durationMs = Integer.parseInt(token.substring(colon + 1));
				} catch (NumberFormatException e) {
					return null;
				}
			}
			durationMs = Math.max(MIN_NOTE_MS, Math.min(MAX_NOTE_MS, durationMs));
			double frequency = noteFrequency(notePart);
			if (frequency < 0) return null;
			return new Note(frequency, durationMs);
		}

		private static double noteFrequency(String noteToken) {
			if (noteToken == null) return -1;
			String token = noteToken.trim();
			if (token.isEmpty()) return -1;
			char noteName = Character.toUpperCase(token.charAt(0));
			if (noteName == 'R') return 0;
			int semitone = noteIndex(noteName);
			if (semitone < 0) return -1;
			int index = 1;
			if (index < token.length()) {
				char accidental = token.charAt(index);
				if (accidental == '#') {
					semitone += 1;
					index++;
				} else if (accidental == 'b' || accidental == 'B') {
					semitone -= 1;
					index++;
				}
			}
			int octave = 4;
			if (index < token.length()) {
				try {
					octave = Integer.parseInt(token.substring(index));
				} catch (NumberFormatException e) {
					return -1;
				}
			}
			if (octave < 0 || octave > 8) return -1;
			int midi = (octave + 1) * 12 + semitone;
			return 440.0 * Math.pow(2.0, (midi - 69) / 12.0);
		}

		private static int noteIndex(char note) {
			switch (note) {
				case 'C':
					return 0;
				case 'D':
					return 2;
				case 'E':
					return 4;
				case 'F':
					return 5;
				case 'G':
					return 7;
				case 'A':
					return 9;
				case 'B':
					return 11;
				default:
					return -1;
			}
		}

		private static void appendTone(ByteArrayOutputStream out, double frequencyHz, int durationMs) {
			int samples = Math.max(1, Math.round(SAMPLE_RATE * durationMs / 1000f));
			int fadeSamples = Math.min(samples / 2, Math.round(SAMPLE_RATE * 0.008f));
			for (int i = 0; i < samples; i++) {
				double envelope = 1.0;
				if (fadeSamples > 0) {
					envelope = Math.min(envelope, i / (double) fadeSamples);
					envelope = Math.min(envelope, (samples - 1 - i) / (double) fadeSamples);
				}
				int sample = 0;
				if (frequencyHz > 0) {
					double wave = Math.sin(2.0 * Math.PI * frequencyHz * i / SAMPLE_RATE);
					sample = (int) Math.round(wave * envelope * TONE_AMPLITUDE * 32767.0);
				}
				out.write(sample & 0xff);
				out.write((sample >> 8) & 0xff);
			}
		}

		private static final class Note {
			final double frequencyHz;
			final int durationMs;

			Note(double frequencyHz, int durationMs) {
				this.frequencyHz = frequencyHz;
				this.durationMs = durationMs;
			}
		}
	}

	private static final class TimerStore implements Closeable {
		private static final String KEY_TIMERS = "timers";
		private final SharedPreferences prefs;
		private final Handler main;
		private final TimerListener listener;
		private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
		private final Map<Integer, ScheduledFuture<?>> futures = new ConcurrentHashMap<>();
		private final List<Timer> timers = new ArrayList<>();
		private int nextId = 1;

		TimerStore(Context context, Handler main, TimerListener listener) {
			this.prefs = context.getSharedPreferences(PREFS, MODE_PRIVATE);
			this.main = main;
			this.listener = listener;
			load();
		}

		synchronized String add(int durationSeconds, String name) {
			if (durationSeconds <= 0) return "Missing duration_seconds";
			long now = System.currentTimeMillis();
			Timer timer = new Timer(nextId++, cleanName(name), durationSeconds, now, now + durationSeconds * 1000L);
			timers.add(timer);
			save();
			schedule(timer);
			return list();
		}

		synchronized String list() {
			try {
				return toJson().toString();
			} catch (JSONException e) {
				return "{\"timers\":[]}";
			}
		}

		synchronized String cancel(JSONObject args) {
			boolean all = args.optBoolean("all", false);
			Integer id = args.has("id") ? args.optInt("id") : null;
			String name = cleanName(args.optString("name", ""));
			if (all) {
				for (Timer timer : timers) cancelFuture(timer.id);
				timers.clear();
				save();
				return list();
			}
			Timer timer = find(id, name);
			if (timer == null) return "No matching timer";
			cancelFuture(timer.id);
			timers.remove(timer);
			save();
			return list();
		}

		synchronized String extend(JSONObject args) {
			int delta = args.optInt("delta_seconds", 0);
			if (delta == 0) return "Missing delta_seconds";
			Integer id = args.has("id") ? args.optInt("id") : null;
			String name = cleanName(args.optString("name", ""));
			Timer timer = find(id, name);
			if (timer == null) return "No matching timer";
			timer.endsAtMs = Math.max(System.currentTimeMillis() + 1000L, timer.endsAtMs + delta * 1000L);
			cancelFuture(timer.id);
			schedule(timer);
			save();
			return list();
		}

		private Timer find(Integer id, String name) {
			if (id != null) {
				for (Timer timer : timers) if (timer.id == id) return timer;
				return null;
			}
			if (!name.isEmpty()) {
				for (Timer timer : timers) if (timer.name.equalsIgnoreCase(name)) return timer;
				return null;
			}
			return timers.size() == 1 ? timers.get(0) : null;
		}

		private void schedule(Timer timer) {
			long delayMs = Math.max(0, timer.endsAtMs - System.currentTimeMillis());
			ScheduledFuture<?> future = scheduler.schedule(() -> {
				synchronized (TimerStore.this) {
					timers.remove(timer);
					futures.remove(timer.id);
					save();
				}
				main.post(() -> listener.onTimer(timer));
			}, delayMs, TimeUnit.MILLISECONDS);
			futures.put(timer.id, future);
		}

		private void cancelFuture(int id) {
			ScheduledFuture<?> future = futures.remove(id);
			if (future != null) future.cancel(false);
		}

		private JSONObject toJson() throws JSONException {
			JSONArray array = new JSONArray();
			long now = System.currentTimeMillis();
			for (Timer timer : timers) {
				JSONObject item = new JSONObject();
				item.put("id", timer.id);
				item.put("name", timer.name);
				item.put("duration_seconds", timer.durationSeconds);
				item.put("remaining_seconds", Math.max(0, (timer.endsAtMs - now + 999) / 1000));
				item.put("created_at_epoch", timer.createdAtMs / 1000);
				array.put(item);
			}
			JSONObject root = new JSONObject();
			root.put("timers", array);
			return root;
		}

		private void save() {
			JSONArray array = new JSONArray();
			try {
				for (Timer timer : timers) {
					JSONObject item = new JSONObject();
					item.put("id", timer.id);
					item.put("name", timer.name);
					item.put("duration", timer.durationSeconds);
					item.put("created", timer.createdAtMs);
					item.put("ends", timer.endsAtMs);
					array.put(item);
				}
			} catch (JSONException ignored) {
			}
			prefs.edit().putString(KEY_TIMERS, array.toString()).putInt("nextTimerId", nextId).apply();
		}

		private void load() {
			nextId = prefs.getInt("nextTimerId", 1);
			long now = System.currentTimeMillis();
			try {
				JSONArray array = new JSONArray(prefs.getString(KEY_TIMERS, "[]"));
				for (int i = 0; i < array.length(); i++) {
					JSONObject item = array.getJSONObject(i);
					Timer timer = new Timer(
						item.optInt("id", nextId++),
						item.optString("name", ""),
						item.optInt("duration", 0),
						item.optLong("created", now),
						item.optLong("ends", now)
					);
					if (timer.endsAtMs > now) {
						timers.add(timer);
						schedule(timer);
						nextId = Math.max(nextId, timer.id + 1);
					}
				}
			} catch (JSONException ignored) {
			}
			save();
		}

		private String cleanName(String name) {
			return name == null ? "" : name.trim();
		}

		@Override
		public void close() {
			for (Integer id : new ArrayList<>(futures.keySet())) cancelFuture(id);
			scheduler.shutdownNow();
		}

		interface TimerListener {
			void onTimer(Timer timer);
		}

		static final class Timer {
			final int id;
			final String name;
			final int durationSeconds;
			final long createdAtMs;
			long endsAtMs;

			Timer(int id, String name, int durationSeconds, long createdAtMs, long endsAtMs) {
				this.id = id;
				this.name = name;
				this.durationSeconds = durationSeconds;
				this.createdAtMs = createdAtMs;
				this.endsAtMs = endsAtMs;
			}
		}
	}

	private static final class ChatWebSocket implements Closeable {
		interface Listener {
			void onOpen();
			void onText(String text);
			void onBinary(byte[] data);
			void onClosed(String reason);
			void onError(Exception error);
		}

		private final URI uri;
		private final String deviceToken;
		private final Listener listener;
		private final SecureRandom random = new SecureRandom();
		private final ExecutorService writer = Executors.newSingleThreadExecutor();
		private volatile boolean open = false;
		private Socket socket;
		private InputStream in;
		private OutputStream out;

		ChatWebSocket(URI uri, String deviceToken, Listener listener) {
			this.uri = uri;
			this.deviceToken = deviceToken == null ? "" : deviceToken;
			this.listener = listener;
		}

		boolean isOpen() {
			return open;
		}

		void connect() {
			try {
				int port = uri.getPort();
				boolean secure = "wss".equals(uri.getScheme());
				if (port < 0) port = secure ? 443 : 80;
				SocketFactory factory = secure ? SSLSocketFactory.getDefault() : SocketFactory.getDefault();
				socket = factory.createSocket(uri.getHost(), port);
				socket.setTcpNoDelay(true);
				in = socket.getInputStream();
				out = socket.getOutputStream();
				handshake();
				open = true;
				listener.onOpen();
				readLoop();
			} catch (Exception e) {
				open = false;
				listener.onError(e);
				closeQuietly(socket);
			}
		}

		private void handshake() throws Exception {
			byte[] nonce = new byte[16];
			random.nextBytes(nonce);
			String key = Base64.encodeToString(nonce, Base64.NO_WRAP);
			String path = uri.getRawPath();
			if (path == null || path.isEmpty()) path = "/";
			if (uri.getRawQuery() != null) path += "?" + uri.getRawQuery();
			String host = uri.getHost() + (uri.getPort() > 0 ? ":" + uri.getPort() : "");
			StringBuilder request = new StringBuilder();
			request.append("GET ").append(path).append(" HTTP/1.1\r\n");
			request.append("Host: ").append(host).append("\r\n");
			request.append("Upgrade: websocket\r\n");
			request.append("Connection: Upgrade\r\n");
			request.append("Sec-WebSocket-Key: ").append(key).append("\r\n");
			request.append("Sec-WebSocket-Version: 13\r\n");
			if (!deviceToken.isEmpty()) {
				request.append("X-Device-Token: ").append(deviceToken).append("\r\n");
			}
			request.append("\r\n");
			out.write(request.toString().getBytes(StandardCharsets.US_ASCII));
			out.flush();

			ByteArrayOutputStream headerBytes = new ByteArrayOutputStream();
			int previous = -1;
			int current;
			while ((current = in.read()) != -1) {
				headerBytes.write(current);
				String headers = headerBytes.toString("ISO-8859-1");
				if (previous == '\r' && current == '\n' && headers.endsWith("\r\n\r\n")) break;
				previous = current;
			}
			String headers = headerBytes.toString("ISO-8859-1");
			if (!headers.startsWith("HTTP/1.1 101")) {
				throw new IOException(headers.split("\r\n", 2)[0]);
			}
			String expected = Base64.encodeToString(
				MessageDigest.getInstance("SHA-1").digest((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").getBytes(StandardCharsets.US_ASCII)),
				Base64.NO_WRAP
			);
			if (!headers.toLowerCase(Locale.US).contains("sec-websocket-accept: " + expected.toLowerCase(Locale.US))) {
				throw new IOException("Bad websocket accept header");
			}
		}

		private void readLoop() throws IOException {
			while (open) {
				int first = in.read();
				if (first < 0) throw new EOFException();
				int second = readByte();
				int opcode = first & 0x0f;
				long length = second & 0x7f;
				if (length == 126) {
					length = (readByte() << 8) | readByte();
				} else if (length == 127) {
					length = 0;
					for (int i = 0; i < 8; i++) {
						length = (length << 8) | readByte();
					}
				}
				byte[] mask = null;
				if ((second & 0x80) != 0) {
					mask = readFully(4);
				}
				if (length > Integer.MAX_VALUE) throw new IOException("Frame too large");
				byte[] payload = readFully((int) length);
				if (mask != null) {
					for (int i = 0; i < payload.length; i++) {
						payload[i] = (byte) (payload[i] ^ mask[i % 4]);
					}
				}
				if (opcode == 0x1) {
					listener.onText(new String(payload, StandardCharsets.UTF_8));
				} else if (opcode == 0x2) {
					listener.onBinary(payload);
				} else if (opcode == 0x8) {
					open = false;
					listener.onClosed("server closed");
					return;
				} else if (opcode == 0x9) {
					queueFrame(0xA, payload);
				}
			}
		}

		void sendText(String text) {
			queueFrame(0x1, text.getBytes(StandardCharsets.UTF_8));
		}

		void sendBinary(byte[] bytes) {
			queueFrame(0x2, bytes);
		}

		private void queueFrame(int opcode, byte[] payload) {
			if (out == null || (!open && opcode != 0x8)) return;
			if (writer.isShutdown()) return;
			try {
				writer.execute(() -> sendFrame(opcode, payload));
			} catch (RuntimeException ignored) {
			}
		}

		private synchronized void sendFrame(int opcode, byte[] payload) {
			if (out == null || !open && opcode != 0x8) return;
			try {
				ByteArrayOutputStream frame = new ByteArrayOutputStream();
				frame.write(0x80 | opcode);
				int length = payload.length;
				if (length <= 125) {
					frame.write(0x80 | length);
				} else if (length <= 65535) {
					frame.write(0x80 | 126);
					frame.write((length >> 8) & 0xff);
					frame.write(length & 0xff);
				} else {
					frame.write(0x80 | 127);
					for (int i = 7; i >= 0; i--) frame.write((length >> (8 * i)) & 0xff);
				}
				byte[] mask = new byte[4];
				random.nextBytes(mask);
				frame.write(mask);
				for (int i = 0; i < payload.length; i++) {
					frame.write(payload[i] ^ mask[i % 4]);
				}
				out.write(frame.toByteArray());
				out.flush();
			} catch (IOException ignored) {
				open = false;
			}
		}

		private int readByte() throws IOException {
			int value = in.read();
			if (value < 0) throw new EOFException();
			return value;
		}

		private byte[] readFully(int length) throws IOException {
			byte[] data = new byte[length];
			int offset = 0;
			while (offset < length) {
				int read = in.read(data, offset, length - offset);
				if (read < 0) throw new EOFException();
				offset += read;
			}
			return data;
		}

		@Override
		public void close() {
			close("client closed");
		}

		void close(String reason) {
			boolean wasOpen = open;
			open = false;
			try {
				queueFrame(0x8, reason.getBytes(StandardCharsets.UTF_8));
			} catch (Exception ignored) {
			}
			closeQuietly(socket);
			writer.shutdownNow();
			if (wasOpen) listener.onClosed(reason);
		}

		private static void closeQuietly(Closeable closeable) {
			if (closeable == null) return;
			try {
				closeable.close();
			} catch (IOException ignored) {
			}
		}
	}
}
