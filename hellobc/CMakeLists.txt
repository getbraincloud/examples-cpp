# CMakeList.txt : CMake project for hellobc, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable C++11 features
set(CMAKE_CXX_STANDARD 11)

project ("hellobc")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT hellobc)

# Add source to this project's executable.
list(APPEND src_files
	main.cpp
	ids.h
	)

add_executable (hellobc ${src_files})

# brainCloud
option(SSL_ALLOW_SELFSIGNED "Will define BC_SSL_ALLOW_SELFSIGNED" ON)
add_subdirectory(../thirdparties/braincloud-cpp ./thirdparties/braincloud-cpp)
list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/include/)
list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/lib/jsoncpp-1.0.0/)
list(APPEND libs brainCloud)

# Setup includes and linked libraries
target_include_directories(hellobc ${includes})
target_link_libraries(hellobc ${libs})