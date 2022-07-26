# brainCloud Relay Test App
brainCloud Relay Service example, written in C++.

![](./screenshots/screenshot.jpg)

## How to Build
First you'll need to properly clone the project's submodules.
```
git submodule update --init
```
* [Build with CMake](#build-with-cmake)
* [Use brainCloud cocoapods](#use-braincloud-cocoapods)

Then, create a file into /src folder called `ids.h`, and put 3 defines in it:
```
#define BRAINCLOUD_SERVER_URL "https://api.braincloudservers.com/dispatcherV2"
#define BRAINCLOUD_APP_ID ""
#define BRAINCLOUD_APP_SECRET ""
```

Fill in values for BRAINCLOUD_APP_ID and BRAINCLOUD_APP_SECRET.

### Build with CMake

1. Download and install cmake from: https://cmake.org.
   Or from terminal:

   Linux:
   ```
   sudo apt-get install cmake
   ```
   Mac:
   ```
   brew install cmake
   ```

2. Create a `build` directory, then `cd` to it.
   ```
   mkdir build
   cd ./build
   ```
3. Generate project
   ```
   cmake ..
   ```
   To generate an XCode project, add the `Xcode` generator:
   ```
   cmake -G Xcode ..
   ```
4. Open the project then compile and run. If makefiles were used (Default CMake on non-windows), do this:
   ```
   make
   ./RelayTestApp
   ```

### Use brainCloud cocoapods

Not currently set up for this project. See BCChat.
