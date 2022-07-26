# Android
brainCloud Android authentication example, written in C++.

![](./screenshots/screenshot.png)

## How to Build
First you'll need to properly clone the project's submodules.
```
git submodule update --init
```

Then, create a file into /src folder called `ids.h`, and put 3 defines in it:
```
#define BRAINCLOUD_SERVER_URL "https://api.braincloudservers.com/dispatcherV2"
#define BRAINCLOUD_APP_ID ""
#define BRAINCLOUD_APP_SECRET ""
```

Fill in values for BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET.

Files to note:

- android/app/src/main/AndroidManifest.xml
- android/app/src/main/java/com/bitheads/braincloud/android/MainActivity.java
- android/app/src/main/cpp/CMakeLists.txt
- android/app/src/main/cpp/native-lib.cpp

### Build in Android Studio

In Android Studio, open the folder **examples-cpp/android**. Run in emulator or on device.

Note: cmake version updated to 3.18.1, gradle plugin version to 7.2.0

In project structure > gradle version is 7.3.3, compile sdk 32, Tools version 32.1.0 rc1, NDK version 21.4.7075529, Java version 1.8 (Java 8)
