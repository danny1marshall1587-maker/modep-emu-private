#!/usr/bin/env bash
set -e

WORK="/tmp/android-kiosk-m11"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$WORK/classes" "$WORK/assets/web" "$WORK/apk_raw"
cp -r "$DIR/src" "$WORK/"
cp "$DIR/AndroidManifest.xml" "$WORK/"

# 1. Fetch official Android d8.jar from Google repository if not cached
if [ ! -f "$WORK/d8.jar" ]; then
    echo "Fetching official Android d8.jar..."
    curl -sL https://dl.google.com/android/repository/build-tools_r30.0.3-linux.zip -o "$WORK/bt.zip"
    unzip -p "$WORK/bt.zip" "android-11/lib/d8.jar" > "$WORK/d8.jar"
    rm -f "$WORK/bt.zip"
fi

# 2. Fetch official Google Android SDK android.jar (API 30) so all method descriptors (e.g. View.setLayerType(int, Paint)) match Android OS runtime 1:1
if [ ! -f "$WORK/android.jar" ]; then
    echo "Fetching official Google Android SDK android.jar (API 30)..."
    curl -sL https://dl.google.com/android/repository/platform-30_r03.zip -o "$WORK/platform.zip"
    unzip -p "$WORK/platform.zip" "android-11/android.jar" > "$WORK/android.jar"
    rm -f "$WORK/platform.zip"
fi

echo "Compiling MainActivity.java against official Google android.jar..."
rm -rf "$WORK/classes" && mkdir -p "$WORK/classes"
javac --release 8 -cp "$WORK/android.jar" -d "$WORK/classes" "$WORK/src/com/patchbox/prime/MainActivity.java"

echo "Converting MainActivity bytecode to DEX via d8..."
rm -f "$WORK/classes.dex"
java -cp "$WORK/d8.jar" com.android.tools.r8.D8 \
    --lib "$WORK/android.jar" \
    --min-api 24 \
    --output "$WORK" \
    "$WORK/classes/com/patchbox/prime/"*.class

echo "Bundling full Patchbox Prime Web Console assets for Lenovo Tab M11..."
rm -rf "$WORK/assets/web"
mkdir -p "$WORK/assets/web"
cp -r /home/patch/modep-live-console-release/web/index.html \
      /home/patch/modep-live-console-release/web/manifest.json \
      /home/patch/modep-live-console-release/web/modgui-host.html \
      /home/patch/modep-live-console-release/web/css \
      /home/patch/modep-live-console-release/web/js \
      /home/patch/modep-live-console-release/web/img \
      "$WORK/assets/web/" 2>/dev/null || true

echo "Packaging APK with aapt (with uncompressed 4-byte aligned resources.arsc for Android 11-14)..."
rm -f "$WORK/unaligned.apk" "$WORK/aligned.apk" "$WORK/PatchboxPrime.apk"
mkdir -p "$WORK/res/values"
printf '<resources><string name="app_name">Patchbox Prime M11</string></resources>\n' > "$WORK/res/values/strings.xml"
aapt package -f \
    -M "$WORK/AndroidManifest.xml" \
    -S "$WORK/res" \
    -I "$WORK/android.jar" \
    -A "$WORK/assets" \
    -0 arsc \
    -F "$WORK/unaligned.apk"

cd "$WORK"
aapt add unaligned.apk classes.dex >/dev/null

echo "Aligning APK with zipalign..."
zipalign -p -f 4 "$WORK/unaligned.apk" "$WORK/aligned.apk"

KEYSTORE="$DIR/debug.keystore"
if [ ! -f "$KEYSTORE" ]; then
    if [ -f "$WORK/debug.keystore" ]; then
        cp "$WORK/debug.keystore" "$KEYSTORE"
    else
        keytool -genkeypair -v \
            -keystore "$KEYSTORE" \
            -storepass android -keypass android \
            -alias androiddebugkey \
            -keyalg RSA -keysize 2048 -validity 10000 \
            -dname "CN=Patchbox Prime M11,O=MODEP,C=GB" >/dev/null 2>&1
    fi
fi

echo "Signing PatchboxPrime.apk with apksigner (v1 + v2 + v3 signatures)..."
apksigner sign \
    --min-sdk-version 19 \
    --v1-signing-enabled true \
    --v2-signing-enabled true \
    --v3-signing-enabled true \
    --ks "$KEYSTORE" \
    --ks-pass pass:android \
    --key-pass pass:android \
    --out "$WORK/PatchboxPrime.apk" \
    "$WORK/aligned.apk"

apksigner verify --verbose --min-sdk-version 19 "$WORK/PatchboxPrime.apk" | head -n 8
cp "$WORK/PatchboxPrime.apk" /home/patch/modep-live-console-release/web/PatchboxPrime.apk
cp "$WORK/PatchboxPrime.apk" /home/patch/modep-live-console-release/web/web/PatchboxPrime.apk 2>/dev/null || true
cp "$WORK/PatchboxPrime.apk" /home/patch/modep-git-backup/web/PatchboxPrime.apk 2>/dev/null || true
cp "$WORK/PatchboxPrime.apk" /home/patch/modep-git-backup/web/web/PatchboxPrime.apk 2>/dev/null || true
cp "$WORK/PatchboxPrime.apk" /var/modep/web/PatchboxPrime.apk 2>/dev/null || true
ls -lh /home/patch/modep-live-console-release/web/PatchboxPrime.apk /var/modep/web/PatchboxPrime.apk
echo "PATCHBOX_PRIME_M11_APK_BUILD_OK"
