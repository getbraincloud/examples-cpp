# Android
brainCloud Android authentication example, written in C++.

![](./Screenshots/android-GamePlay.png)

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

Note: cmake version updated to 3.22.1, gradle plugin version to 8.0.2

In project structure > gradle version is 8.0, compile sdk 33, Tools version 34, NDK version 25.1.8937393, Java version 11 (Java 11)
