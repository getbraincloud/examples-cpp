# Copyright 2018 bitHeads inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if (CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
    set(UWP 1)
endif()

if (UWP)
    # We require 3.4.0 for UWP generation
    cmake_minimum_required(VERSION 3.4.0)
else()
    cmake_minimum_required(VERSION 3.0.0)
endif()

# Project
project(BCChat)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BCChat)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY XCODE_STARTUP_PROJECT BCChat)

# Enable C++11 features
set(CMAKE_CXX_STANDARD 11)

# Some Win32 definitions to avoid common issues
if (WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
if (UWP)
    add_definitions(
        -DBCCHAT_UWP
        -DIMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
        -DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS)
endif()
# for MacOS glInteger warnings, however this adds multiple definition warning in imgui
if (APPLE)
    add_definitions(-DGL_SILENCE_DEPRECATION)
    # enable objc for SDL
    enable_language(OBJC)
endif()

if(DEFINED SSL_ALLOW_SELFSIGNED)
    set(SSL_ALLOW_SELFSIGNED 1)
else()
    set(SSL_ALLOW_SELFSIGNED 0)
endif()

if (SSL_ALLOW_SELFSIGNED)
    message("brainCloud Skipping certificate verification in RTT")
    add_definitions(-DBC_SSL_ALLOW_SELFSIGNED)
    else()
    message("brainCloud Building with certificates for RTT")
endif()

# Application source files
list(APPEND src_files
    src/ids.h
    src/globals.cpp
    src/globals.h
    src/app.cpp
    src/app.h
    src/login.cpp
    src/login.h
    src/chat.cpp
    src/chat.h
    src/loading.cpp
    src/loading.h
    src/askForName.cpp
    src/askForName.h
    src/menuBar.cpp
    src/menuBar.h
    src/BCCallback.cpp
    src/BCCallback.h
)

if (UWP)
    list(APPEND src_files
        src/mainUWP.cpp
    )
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/uwp_res/package.appxManifest 
                   ${CMAKE_BINARY_DIR}/BCChat.dir/package.appxManifest COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/uwp_res/Logo.png 
                    ${CMAKE_BINARY_DIR}/BCChat.dir/Logo.png COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/uwp_res/SmallLogo.png 
                    ${CMAKE_BINARY_DIR}/BCChat.dir/SmallLogo.png COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/uwp_res/SmallLogo44x44.png 
                   ${CMAKE_BINARY_DIR}/BCChat.dir/SmallLogo44x44.png COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/uwp_res/SplashScreen.png 
                    ${CMAKE_BINARY_DIR}/BCChat.dir/SplashScreen.png COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/uwp_res/StoreLogo.png 
                    ${CMAKE_BINARY_DIR}/BCChat.dir/StoreLogo.png COPYONLY)
    list(APPEND res_src_files
        ${CMAKE_BINARY_DIR}/BCChat.dir//package.appxManifest
        ${CMAKE_BINARY_DIR}/BCChat.dir//Logo.png
        ${CMAKE_BINARY_DIR}/BCChat.dir//SmallLogo.png
        ${CMAKE_BINARY_DIR}/BCChat.dir//SmallLogo44x44.png
        ${CMAKE_BINARY_DIR}/BCChat.dir//SplashScreen.png
        ${CMAKE_BINARY_DIR}/BCChat.dir//StoreLogo.png
    )
    set_source_files_properties(
        "src/app.cpp"
        "src/mainUWP.cpp"
        PROPERTIES COMPILE_FLAGS "/ZW")
else()
    list(APPEND src_files src/mainSDL.cpp)
endif()

# Third parties source files
file(GLOB thirdparty_src_files
        ../thirdparties/imgui/*.cpp
        ../thirdparties/imgui/*.h
        )

if (UWP)
    list(APPEND thirdparty_src_files
        src/imgui/imgui_impl_dx11.cpp
        src/imgui/imgui_impl_dx11.h
    )
else()
    list(APPEND thirdparty_src_files
                ../thirdparties/imgui/backends/imgui_impl_opengl2.cpp
                ../thirdparties/imgui/backends/imgui_impl_opengl2.h
                ../thirdparties/imgui/backends/imgui_impl_sdl3.cpp
                ../thirdparties/imgui/backends/imgui_impl_sdl3.h
    )
endif()

# Separate filters IDEs
source_group("bcchat" FILES ${src_files})
source_group("thirdparty" FILES ${thirdparty_src_files})
if (UWP)
    source_group("Resource Files" FILES ${res_src_files})
endif()

# SDL
if (NOT UWP)
    set(HAVE_LIBC OFF)
    add_subdirectory(../thirdparties/SDL ./thirdparties/SDL)
    list(APPEND libs SDL3-shared)
    list(APPEND includes PUBLIC ../thirdparties/SDL/include/)
endif()

# BCChat
if (UWP)
    add_executable(BCChat WIN32 ${src_files} ${thirdparty_src_files} ${res_src_files})
    set_property(TARGET BCChat APPEND_STRING PROPERTY LINK_FLAGS_DEBUG 
        " /nodefaultlib:vccorlibd /nodefaultlib:msvcrtd vccorlibd.lib msvcrtd.lib")
    set_property(TARGET BCChat APPEND_STRING PROPERTY LINK_FLAGS_RELEASE 
        " /nodefaultlib:vccorlib /nodefaultlib:msvcrt vccorlib.lib msvcrt.lib")
else()
    add_executable(BCChat ${src_files} ${thirdparty_src_files})
endif()
list(APPEND includes PUBLIC ./src/)

# OpenGL
if (NOT UWP)
    add_definitions(-DGL_SILENCE_DEPRECATION)
    find_package(OpenGL REQUIRED)
    list(APPEND includes PUBLIC ${OPENGL_INCLUDE_DIR})
    list(APPEND libs ${OPENGL_LIBRARIES})
endif()

# brainCloud
add_subdirectory(../thirdparties/braincloud-cpp ./thirdparties/braincloud-cpp)
list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/include/)
list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/lib/jsoncpp-1.0.0/)
list(APPEND libs brainCloud)

# ImGui
list(APPEND includes PUBLIC ../thirdparties/imgui/)
list(APPEND includes PUBLIC ../thirdparties/imgui/backends)

# Add a post-build command to copy the DLL
add_custom_command(TARGET BCChat POST_BUILD
    COMMAND cmake -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/thirdparties/SDL/${CMAKE_BUILD_TYPE}/SDL3.dll"
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/SDL3.dll"
    COMMENT "Copying SDL3.dll to the output directory from ${SDL3_DLL_PATH} "
)

# Setup includes and linked libraries
target_include_directories(BCChat ${includes})
target_link_libraries(BCChat ${libs})
