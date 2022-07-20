# C++ Examples

This repository contains example C++ projects that use the brainCloud client. This is a good place to start learning how the various brainCloud APIs can be used.

## BrainCloud-Cpp

The C++ client library is contained in folder thirdparties/braincloud-cpp. It is ready to build with cmake (minimum 3.4.0) for every platform. It can be built at the commandline as a static library or included in a project in your favourite IDE like android studio, xcode, visual studio or rider. Find the latest releases of the C++ brainCloud client library [here](https://github.com/getbraincloud/braincloud-cpp).

### Clone:

The projects include the required third party libraries and braincloud-cpp as git submodules.

```
$ git clone --recurse-submodules git@github.com:getbraincloud/examples-cpp.git
```

If you are updating an existing repository, or forget to --recurse-submodules, then just update the modules. 
Make sure you've done this if you get "missing plugin" error on load.

```
$ cd examples-cpp
$ git submodule update --init --recursive
```

### ids.h

Most examples require the header file ** ids.h ** which defines the server url, app id and app secret. This file is not included with the examples and needs to be copied from somewhere else or created.

## Android

In Android Studio, open the folder ** examples-cpp/android **. Run in emulator or on device.

Files to note:

- android/app/src/main/AndroidManifest.xml
- android/app/src/main/java/com/bitheads/braincloud/android/MainActivity.java
- android/app/src/main/cpp/CMakeLists.txt
- android/app/src/main/cpp/native-lib.cpp


## BC Chat

## Gamelift

## Relay Test App

## Room Server
