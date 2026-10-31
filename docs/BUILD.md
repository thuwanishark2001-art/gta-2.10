# Building the Kingmakers RP client (debug APK)

## Requirements

| Tool | Version |
|---|---|
| JDK | 17 or 21 (AGP 8.4 needs 17+) |
| Gradle | 8.9 (via `./gradlew`, downloaded automatically) |
| Android Gradle Plugin | 8.4.0 |
| Android SDK platform | `platforms;android-34` (compileSdk/targetSdk 34, minSdk 26) |
| Build tools | `build-tools;34.0.0` |
| NDK | `ndk;26.2.11394342` |
| CMake | `cmake;3.22.1` (any 3.12+ works) |
| ABIs | `arm64-v8a`, `armeabi-v7a` |

## Steps

```bash
# 1. Install the SDK packages (once)
sdkmanager "platforms;android-34" "build-tools;34.0.0" "platform-tools" \
           "ndk;26.2.11394342" "cmake;3.22.1"

# 2. Point Gradle at the SDK
echo "sdk.dir=/path/to/android-sdk" > local.properties

# 3. Build
./gradlew assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk` (about 33 MB).

The APK doesn't contain GTA: San Andreas. To run it, the device needs
GTA: San Andreas for Android **version 2.10** installed from Google Play.
On first launch the app copies the engine from that install and reads the
game data from it (see `com/samp/mobile/game/GameFiles.java`).

Debug builds are signed with the Android SDK's default debug key. Release
signing reads `keystore.properties` in the repo root, which is git-ignored:

```properties
storeFile=/absolute/path/to/your-release.jks
storePassword=...
keyAlias=...
keyPassword=...
```

## Choosing the server

The client connects to `KM_SERVER_HOST:KM_SERVER_PORT` (see
`app/src/main/cpp/samp/servers.h`). The default is `10.0.2.2:7777`, which is
the host machine as seen from the Android emulator. To build for another
server, add CMake arguments in `app/build.gradle`:

```groovy
arguments "-DANDROID_STL=c++_shared", "-DKM_SERVER_HOST=192.168.1.20", "-DKM_SERVER_PORT=7777"
```

## Known build gotchas

- **`[CXX1210] No compatible library found`**: if your environment sets
  `JAVA_TOOL_OPTIONS`, the JVM prints "Picked up JAVA_TOOL_OPTIONS" to stderr
  and AGP's prefab step treats that as a failure. Unset it for the build
  (`env -u JAVA_TOOL_OPTIONS ./gradlew assembleDebug`) and put any proxy
  settings in `~/.gradle/gradle.properties` as `systemProp.*` instead.
- Includes are case sensitive on Linux and macOS. The upstream code was built
  on Windows, so a wrong-case `#include` shows up as "file not found".
