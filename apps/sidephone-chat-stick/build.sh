#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}"
PLATFORM="${ANDROID_PLATFORM:-android-35}"
BUILD_TOOLS="${ANDROID_BUILD_TOOLS:-$SDK/build-tools/35.0.0}"
ANDROID_JAR="$SDK/platforms/$PLATFORM/android.jar"

if [ ! -f "$ANDROID_JAR" ]; then
	echo "Missing Android platform: $ANDROID_JAR" >&2
	exit 1
fi

for tool in aapt2 d8 zipalign apksigner; do
	if [ ! -x "$BUILD_TOOLS/$tool" ]; then
		echo "Missing build tool: $BUILD_TOOLS/$tool" >&2
		exit 1
	fi
done

KEYSTORE="$ROOT/.debug.keystore"

rm -rf "$ROOT/build"
mkdir -p "$ROOT/build/compiled" "$ROOT/build/generated" "$ROOT/build/classes" "$ROOT/build/dex" "$ROOT/build/intermediates"

"$BUILD_TOOLS/aapt2" compile --dir "$ROOT/res" -o "$ROOT/build/compiled"
"$BUILD_TOOLS/aapt2" link \
	-o "$ROOT/build/intermediates/resources.apk" \
	-I "$ANDROID_JAR" \
	--manifest "$ROOT/AndroidManifest.xml" \
	--java "$ROOT/build/generated" \
	--auto-add-overlay \
	"$ROOT"/build/compiled/*.flat

JAVA_FILES=()
while IFS= read -r -d '' file; do
	JAVA_FILES+=("$file")
done < <(find "$ROOT/src/main/java" "$ROOT/build/generated" -name '*.java' -print0)

javac -source 11 -target 11 \
	-classpath "$ANDROID_JAR" \
	-d "$ROOT/build/classes" \
	"${JAVA_FILES[@]}"

"$BUILD_TOOLS/d8" --min-api 23 --lib "$ANDROID_JAR" --output "$ROOT/build/dex" $(find "$ROOT/build/classes" -name '*.class')
cp "$ROOT/build/intermediates/resources.apk" "$ROOT/build/intermediates/unsigned.apk"
(cd "$ROOT/build/dex" && zip -q -u "$ROOT/build/intermediates/unsigned.apk" classes.dex)

if [ ! -f "$KEYSTORE" ]; then
	keytool -genkeypair \
		-keystore "$KEYSTORE" \
		-storepass android \
		-keypass android \
		-alias androiddebugkey \
		-keyalg RSA \
		-keysize 2048 \
		-validity 10000 \
		-dname "CN=Android Debug,O=Android,C=US" >/dev/null
fi

"$BUILD_TOOLS/zipalign" -f -p 4 "$ROOT/build/intermediates/unsigned.apk" "$ROOT/build/intermediates/aligned.apk"
"$BUILD_TOOLS/apksigner" sign \
	--ks "$KEYSTORE" \
	--ks-key-alias androiddebugkey \
	--ks-pass pass:android \
	--key-pass pass:android \
	--out "$ROOT/build/chat-stick-sidephone-debug.apk" \
	"$ROOT/build/intermediates/aligned.apk"

"$BUILD_TOOLS/apksigner" verify "$ROOT/build/chat-stick-sidephone-debug.apk"
echo "$ROOT/build/chat-stick-sidephone-debug.apk"
