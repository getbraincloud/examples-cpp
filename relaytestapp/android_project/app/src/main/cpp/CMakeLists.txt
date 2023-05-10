# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.18.1)

# Declares and names the project.

project("relaybot")

set(SRC_ROOT ../../../../..)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
file(GLOB IMGUI_FILES
        ${SRC_ROOT}/../thirdparties/imgui/*.cpp
        ${SRC_ROOT}/../thirdparties/imgui/*.h
        )

add_library( # Sets the name of the library.
        relaybot

        # Sets the library as a shared library.
        SHARED

        ${IMGUI_FILES}
        ${SRC_ROOT}/../thirdparties/imgui/backends/imgui_impl_sdl2.cpp
        ${SRC_ROOT}/../thirdparties/imgui/backends/imgui_impl_sdl2.h
        ${SRC_ROOT}/../thirdparties/imgui/backends/imgui_impl_opengl3.cpp
        ${SRC_ROOT}/../thirdparties/imgui/backends/imgui_impl_opengl3.h

        # Provides a relative path to your source file(s).
        ${SRC_ROOT}/src/ids.h
        ${SRC_ROOT}/src/globals.cpp
        ${SRC_ROOT}/src/globals.h
        ${SRC_ROOT}/src/app.cpp
        ${SRC_ROOT}/src/app.h
        ${SRC_ROOT}/src/game.cpp
        ${SRC_ROOT}/src/game.h
        ${SRC_ROOT}/src/lobby.cpp
        ${SRC_ROOT}/src/lobby.h
        ${SRC_ROOT}/src/login.cpp
        ${SRC_ROOT}/src/login.h
        ${SRC_ROOT}/src/loading.cpp
        ${SRC_ROOT}/src/loading.h
        ${SRC_ROOT}/src/mainMenu.cpp
        ${SRC_ROOT}/src/mainMenu.h
        ${SRC_ROOT}/src/BCCallback.cpp
        ${SRC_ROOT}/src/BCCallback.h
        ${SRC_ROOT}/src/mainSDL.cpp
)

option(SDL_TEST "Descr" OFF)
option(SDL_SHARED "Descr" ON)
#option(SDL_STATIC "Descr" ON)
#option(SDL_DLOPEN "Descr" OFF)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

find_library(SDL2 SDL2)

#add brainCloud subdir
#add_subdirectory(./thirdparty/braincloud-cpp) # add plugin code under source code
add_subdirectory(${SRC_ROOT}/../thirdparties/braincloud-cpp ./binary/braincloud) # add out-of-tree plugin code in project root
add_subdirectory(${SRC_ROOT}/../thirdparties/SDL2 ./binary/SDL2) # add out-of-tree plugin code in project root

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        relaybot

        android

        # GL ES 3, used for the sample renderer
        GLESv3

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}

        brainCloud

        SDL2-static)

target_include_directories(relaybot PUBLIC ${SRC_ROOT}/../thirdparties/SDL2/include/)
target_include_directories(relaybot PRIVATE ${SRC_ROOT}/../thirdparties/imgui/)
target_include_directories(relaybot PRIVATE ${SRC_ROOT}/../thirdparties/imgui/backends/)