# BC Chat
brainCloud RTT Chat example, written in C++.

![](./screenshots/screenshot.jpg)

## How to Build
First you'll need to properly clone the project's submodules.
```
git submodule update --init
```
* [Build with CMake](#build-with-cmake)
* [Use brainCloud cocoapods](#use-braincloud-cocoapods)

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
   ./BCChat
   ```

### Use brainCloud cocoapods

1. Install cocoapods
   ```
   brew install cocoapods
   ```
2. Download SDL2 for Mac OS X from here https://www.libsdl.org/
3. Copy `SDL2.framework` to `/Library/Frameworks/`. (Command+Shift+G in finder)
4. `cd` to the Xcode project.
   ```
   cd ./macOS/
   ```
5. Install pods dependencies for this project.
   ```
   pod install
   ```
6. Open the pre-made Xcode project `./macOS/BCChat.xcodeproj` and run it.
