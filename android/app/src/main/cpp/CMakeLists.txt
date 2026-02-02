# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.18.1)

# Declares and names the project.

project("braincloud_android")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        braincloud_android

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library(
        log-lib
        log
)

find_library(
        android-lib
        android
)

#option(SSL_ALLOW_SELFSIGNED 1)
#add_definitions(-DSSL_ALLOW_SELFSIGNED=ON)

#add brainCloud subdir
#add_subdirectory(./thirdparty/braincloud-cpp) # add plugin code under source code
add_subdirectory(../../../../../thirdparties/braincloud-cpp ./binary/braincloud) # add out-of-tree plugin code in project root

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        braincloud_android

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}
        ${android-lib}

        # Link the target library to the brainCloud library
        brainCloud
        )
	
