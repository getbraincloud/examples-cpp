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

### Setup

Most examples require the header file ids.h which defines the server url, app id and app secret. This file is not included with the examples. Create a file into source code folder called `ids.h`, and put 3 defines in it:

```
#define BRAINCLOUD_SERVER_URL "https://api.braincloudservers.com/dispatcherV2"
#define BRAINCLOUD_APP_ID ""
#define BRAINCLOUD_APP_SECRET ""
```

Fill in values for BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET.

Further information about the brainCloud API, including example Tutorials can be found here:

http://getbraincloud.com/apidocs/

If you haven't signed up or you want to log into the brainCloud portal, you can do that here:

https://portal.braincloudservers.com/

## Android

In Android Studio, open the folder **examples-cpp/android**. Run in emulator or on device.

See Android [Readme](https://github.com/getbraincloud/examples-cpp/blob/develop/android/README.md).

## BC Chat

To build with cocoapods using **examples-cpp/bcchat/MacOS/BCChat.xcworkspace**: 
   ```
   cd ./macOS/
   pod install
   ```

To generate an xcode project to open and build: 
   ```
   mkdir build
   cd ./build
   cmake -G Xcode ..
   ```
   
To generate, build and run makefile project at command prompt:
    ```
    mkdir build
    cd ./build
    cmake ..
    make
   ./BCChat
   ```
   
See BCChat [Readme](https://github.com/getbraincloud/examples-cpp/blob/develop/bcchat/README.md).

## Gamelift

## Relay Test App

## Room Server
